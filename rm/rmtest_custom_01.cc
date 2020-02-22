#include "rm_test_util.h"


void printSlots(std::string fileName) {
    FileHandle fileHandle;
    RecordBasedFileManager::instance().openFile(fileName, fileHandle);
    void *pageData = malloc(PAGE_SIZE);
    for (PageNum i = 0; i < fileHandle.getNumberOfPages(); i++) {
        fileHandle.readPage(i, pageData);
        DataPage p(pageData);
        std::cout << "PAGE " << i << " of file " << fileName << std::endl;
        for (SlotNumber slot = 0; slot < p.getSlotNumber(); slot++) {
            PageOffset recordOffset;
            RecordSize recordLength;
            SlotPointerIndicator isPointer;
            p.parseSlot(slot, isPointer, recordOffset, recordLength);
            std::cout << "slot " << slot << ": " << isPointer << " "
                      << recordOffset << " " << recordLength << "\t";
        }
        std::cout << std::endl;
    }
    free(pageData);
}

TableID getTableID(std::string tableName) {
    TableID tableId;
    std::string fileName;
    RC rc = rm.getTableInfo(tableName, tableId, fileName);
    assert(rc == 0);
    return tableId;
}


RC TEST_RM_CUSTOM_01()
{
    // Functions Tested:
    // 1. Create catalog
    // 2. Create tables
    // 3. Delete a table
    // 4. Create a table
    std::cout <<std::endl << "***** In RM Custom Test Case 01 *****" <<std::endl;

    rm.deleteCatalog();

    RC rc = rm.createCatalog();
    assert(rc == success && "create catalog failed");

    int tableNumber = 100;
    TableID nextTableID = 3;

    std::string tableName = "test_table";
    for (int i = 0; i < tableNumber; i++) {
        std::string curTableName = tableName + std::to_string(i);
        createTable(curTableName);
        TableID tableId = getTableID(curTableName);
        assert(tableId == nextTableID);
        nextTableID++;
        //std::cout << "==== created " << curTableName << "=====" << std::endl;
        //rm.printSysTable(SYSTABLE);
    }

    int deleteNumber = tableNumber / 2;
    for (int i = deleteNumber; i >= 0; i--) {
        std::string curTableName = tableName + std::to_string(i);
        rc = rm.deleteTable(curTableName);
        assert(rc == 0 && "delete table failed");
        //std::cout << "==== deleted " << curTableName << "=====" << std::endl;
        //rm.printSysTable(SYSTABLE);
    }

    int appendNumber = tableNumber / 4;
    for (int i = tableNumber; i < tableNumber + appendNumber; i++) {
        std::string curTableName = tableName + std::to_string(i);
        createTable(curTableName);
        TableID tableId = getTableID(curTableName);
        assert(tableId == nextTableID);
        nextTableID++;
        //std::cout << "==== created " << curTableName << "=====" << std::endl;
        //rm.printSysTable(SYSTABLE);
        //printSlots(SYSTABLE);
        if (i > tableNumber) {
            std::vector<Attribute> attrs;
            rc = rm.getAttributes(tableName + std::to_string(i-1), attrs);
            assert(rc == 0 && "read table failed");
        }
    }

    std::cout << "next table id: " << nextTableID << std::endl;

    std::cout << "***** Custom Test Case 01 Finished. *****" <<std::endl;
    return 0;
}

int main()
{
    return TEST_RM_CUSTOM_01();
}

