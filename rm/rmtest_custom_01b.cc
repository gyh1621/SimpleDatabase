#include "rm_test_util.h"

TableID getTableID(std::string tableName) {
    TableID tableId;
    std::string fileName;
    RC rc = rm.getTableInfo(tableName, tableId, fileName);
    assert(rc == 0);
    return tableId;
}


RC TEST_RM_CUSTOM_01b()
{
    // Functions Tested:
    // reopen rm, test table id
    std::cout <<std::endl << "***** In RM Custom Test Case 1b *****" <<std::endl;

    TableID nextTableID = 128;  // result from custom test case 01
    int tableNumber = 50;
    RC rc;

    std::string tableName = "test_table";
    for (int i = 0; i < tableNumber; i++) {
        std::string curTableName = tableName + std::to_string(nextTableID);
        createTable(curTableName);
        TableID tableId = getTableID(curTableName);
        assert(tableId == nextTableID);
        nextTableID++;
    }

    for (int i = 0; i < nextTableID; i++) {
        rm.deleteTable(tableName + std::to_string(i));
    }

    rm.deleteCatalog();

    std::cout << "***** Custom Test Case 01b Finished. *****" <<std::endl;
    return 0;
}

int main()
{
    return TEST_RM_CUSTOM_01b();
}

