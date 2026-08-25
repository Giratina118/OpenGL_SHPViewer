#include <pch.h>
#include "DbfWriter.h"
#include "DBFTable.h"
#include <fstream>
#include <cstdio>

// DBF ¿˙¿Â
bool DbfWriter::WriteDbf(const std::filesystem::path& path, DBFTable& dbfTable, const std::vector<int32_t>& keepIds)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) return false;

    uint16_t columnCount = static_cast<uint16_t>(dbfTable.columns.size());
    uint16_t headerSize = 33 + columnCount * 32;
    uint16_t recordSize = 1;
    for (const auto& col : dbfTable.columns) recordSize += col.length;

    uint8_t version = 0x03;
    out.write(reinterpret_cast<char*>(&version), 1);
    uint8_t lastUpdate[3] = { 25, 1, 1 };
    out.write(reinterpret_cast<char*>(lastUpdate), 3);
    uint32_t recordCount = static_cast<uint32_t>(keepIds.size());
    out.write(reinterpret_cast<char*>(&recordCount), 4);
    out.write(reinterpret_cast<char*>(&headerSize), 2);
    out.write(reinterpret_cast<char*>(&recordSize), 2);
    uint8_t reserved[20] = {};
    out.write(reinterpret_cast<char*>(reserved), 20);

    for (const auto& col : dbfTable.columns) {
        char name[11] = {};
        strncpy_s(name, col.name.c_str(), 10);
        out.write(name, 11);

        char typeChar = static_cast<char>(col.type);
        out.write(&typeChar, 1);

        uint32_t zero = 0;
        out.write(reinterpret_cast<char*>(&zero), 4);
        out.write(reinterpret_cast<const char*>(&col.length), 1);
        out.write(reinterpret_cast<const char*>(&col.decimals), 1);

        uint8_t reservedField[14] = {};
        out.write(reinterpret_cast<char*>(reservedField), 14);
    }

    uint8_t headerTerminator = 0x0D;
    out.write(reinterpret_cast<char*>(&headerTerminator), 1);

    for (int32_t row : keepIds) {
        char deleteFlag = ' ';
        out.write(&deleteFlag, 1);

        for (const auto& col : dbfTable.columns) {
            std::string field;
            switch (col.type) {
            case DBFColumnType::Int32: {
                char buf[32];
                snprintf(buf, sizeof(buf), "%*d", (int)col.length, dbfTable.intColumns[col.typeIndex][row]);
                field.assign(buf, col.length);
                break;
            }
            case DBFColumnType::Double: {
                char buf[64];
                snprintf(buf, sizeof(buf), "%*.*f", (int)col.length, (int)col.decimals, dbfTable.doubleColumns[col.typeIndex][row]);
                field.assign(buf, col.length);
                break;
            }
            case DBFColumnType::Bool: {
                field.assign(col.length, ' ');
                field[0] = dbfTable.logicalColumns[col.typeIndex][row] ? 'T' : 'F';
                break;
            }
            case DBFColumnType::String: {
                auto view = dbfTable.stringColumns[col.typeIndex].GetView(row);
                field.assign(view.data(), view.size());
                break;
            }
            default:
                field.assign(col.length, ' ');
                break;
            }
            if ((int)field.size() < col.length) field.append(col.length - field.size(), ' ');
            else if ((int)field.size() > col.length) field.resize(col.length);
            out.write(field.data(), col.length);
        }
    }

    uint8_t eof = 0x1A;
    out.write(reinterpret_cast<char*>(&eof), 1);
    return true;
}