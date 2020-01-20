//
// Created by gyh on 2020/1/20.
//

#include "rbfm.h"
#include "test_util.h"

int RBFTest_Custom_3(RecordBasedFileManager &rbfm) {
    // Functions Tested:
    // 1. r/w/a page counters
    std::cout << std::endl << "***** In RBF Test Custom Case 03 *****" << std::endl;

    RC rc;
    std::string fileName = "custom_test3";

    // Create files named "custom_test3"
    rc = rbfm.createFile(fileName);
    assert(rc == success && "Creating the file failed.");

    rc = createFileShouldSucceed(fileName);
    assert(rc == success && "Creating the file failed.");

    // Open the file
    FileHandle fileHandle;
    rc = rbfm.openFile(fileName, fileHandle);
    assert(rc == success && "Opening the file should not fail.");

    RID rid;
    void *record = malloc(1000);
    int numRecords = 20000;

    std::vector <Attribute> recordDescriptor;
    createLargeRecordDescriptor2(recordDescriptor);

    // NULL field indicator
    int nullFieldsIndicatorActualSize = getActualByteForNullsIndicator(recordDescriptor.size());
    auto *nullsIndicator = (unsigned char *) malloc(nullFieldsIndicatorActualSize);
    memset(nullsIndicator, 0, nullFieldsIndicatorActualSize);

    Counter readPageCounter, writePageCounter, appendPageCounter;
    // Insert 20000 records into file
    for (int i = 0; i < numRecords; i++) {
        if (i % 1000 == 0 || i < 100) {
            fileHandle.collectCounterValues(readPageCounter, writePageCounter, appendPageCounter);
            std::cout << "After inserting " << i << " records, r/w/a: " << readPageCounter << "\t"
                      << writePageCounter << "\t" << appendPageCounter << " data page/total page: "
                      << fileHandle.getNumberOfPages() << "\t" << fileHandle.getActualNumberOfPages() << std::endl;
        }
        // Test insert Record
        memset(record, 0, 1000);
        int size = 0;
        prepareLargeRecord2(recordDescriptor.size(), nullsIndicator, i, record, &size);

        rc = rbfm.insertRecord(fileHandle, recordDescriptor, record, rid);
        assert(rc == success && "Inserting a record should not fail.");
    }

    free(record);
    free(nullsIndicator);
    rbfm.closeFile(fileHandle);

    std::cout << "RBF Test Custom Case 03 Finished! The result will not be examined :)" << std::endl << std::endl;

    return 0;
}

int main() {
    // To test the functionality of the paged file manager
    RecordBasedFileManager &rbfm = RecordBasedFileManager::instance();

    // Remove files that might be created by previous test run
    remove("custom_test3");

    return RBFTest_Custom_3(rbfm);
}