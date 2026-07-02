#include <pch.h>
#include "DBFTable.h"

CString DBFTable::PrintAttribute(int32_t dataId) {
    CString text;
    text = "";

    for (ColumnInfo column : columns) {
        text += column.name.c_str();
        text += ": ";

        switch (column.type) {
            case DBFColumnType::Int32:  text += std::to_string(intColumns[column.typeIndex][dataId]).c_str();     break;
            case DBFColumnType::Double: text += std::to_string(doubleColumns[column.typeIndex][dataId]).c_str();  break;
            case DBFColumnType::Bool:   text += std::to_string(logicalColumns[column.typeIndex][dataId]).c_str(); break;
            case DBFColumnType::String: 
            {
                auto view = stringColumns[column.typeIndex].GetTrimmedView(dataId);

                text += CString(view.data(), view.size());
                break;
            }
        }
        text += "\n";
    }

    return text;
}

// .dbf 컬럼 정보
void DBFTable::ReadColumnInfo(const uint8_t*& ptr, int32_t columnInfoNum) {
    ColumnInfo& columnInfo = columns[columnInfoNum];

    char nameTemp[12] = {};
    memcpy(nameTemp, ptr, 11);
    columnInfo.name = nameTemp;
    ptr += 11;

    columnInfo.type = static_cast<DBFColumnType>(*ptr);
    ptr += 1;

    ptr += 4;

    memcpy(&columnInfo.length, ptr, 1);
    ptr += 1;

    memcpy(&columnInfo.decimals, ptr, 1);
    ptr += 1;

    ptr += 14;

    if (columnInfo.type == DBFColumnType::Int32 && columnInfo.decimals != 0)
        columnInfo.type = DBFColumnType::Double;

    switch (columnInfo.type) {
    case DBFColumnType::Int32:  columnInfo.typeIndex = AddIntIndex();     break;
    case DBFColumnType::Double: columnInfo.typeIndex = AddDoubleIndex();  break;
    case DBFColumnType::String: columnInfo.typeIndex = AddStringIndex();  break;
    case DBFColumnType::Bool:   columnInfo.typeIndex = AddBoolIndex();    break;
    }
}