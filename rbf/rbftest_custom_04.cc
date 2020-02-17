#define private public

#include <random>
#include "rbfm.h"
#include "test_util.h"

int RBFTest_Custom_4(RecordBasedFileManager &rbfm) {
    // Functions Tested:
    // 1. free space in FSP matches that computed by Page object
    // 2. no empty space between records
    std::cout << std::endl << "***** In RBF Test Custom Case 04 *****" << std::endl;

    RC rc;
    std::string fileName = "custom_test4";

    // Create files named "custom_test4"
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
    int numRecords = 30000;

    std::vector <Attribute> largeRecordDescriptor;
    createLargeRecordDescriptor2(largeRecordDescriptor);
    std::vector <Attribute> recordDescriptor;
    createRecordDescriptor(recordDescriptor);

    // NULL field indicator
    int nullFieldsIndicatorActualSize = getActualByteForNullsIndicator(recordDescriptor.size());
    auto *nullsIndicator = (unsigned char *) malloc(nullFieldsIndicatorActualSize);
    memset(nullsIndicator, 0, nullFieldsIndicatorActualSize);
    nullFieldsIndicatorActualSize = getActualByteForNullsIndicator(largeRecordDescriptor.size());
    auto *largeNullsIndicator = (unsigned char *) malloc(nullFieldsIndicatorActualSize);
    memset(largeNullsIndicator, 0, nullFieldsIndicatorActualSize);

    // Insert 30000 records into file
    std::vector<RID> rids;
    int size;
    for (int i = 0; i < numRecords; i++) {
        // Test insert Record
        memset(record, 0, 1000);
        prepareLargeRecord2(largeRecordDescriptor.size(), largeNullsIndicator, i, record, &size);
        rc = rbfm.insertRecord(fileHandle, largeRecordDescriptor, record, rid);
        rids.push_back(rid);
        assert(rc == success && "Inserting a record should not fail.");
    }

    rbfm.closeFile(fileHandle);
    rc = rbfm.openFile(fileName, fileHandle);
    assert(rc == success && "Opening the file should not fail.");
    // check free space matches
    checkFreeSpaceMatch(fileHandle);
    // check whether exists empty space
    checkRecordOffsets(fileHandle);

    // update or delete 20000 records
    std::cout << "Update or Delete Records" << std::endl;
    std::random_device dev;
    std::mt19937 engine(dev());
    for (int i = 0; i < 20000; i++) {
        int index1 = engine() % rids.size();
        int index2 = engine() % rids.size();
        if (i % 2 == 0) {
            prepareLargeRecord2(largeRecordDescriptor.size(), largeNullsIndicator, index1, record, &size);
            rc = rbfm.updateRecord(fileHandle, largeRecordDescriptor, record, rids[index2]);
            assert(rc == success && "Update a record should not fail.");
        } else {
            rc = rbfm.deleteRecord(fileHandle, largeRecordDescriptor, rids[index2]);
            assert(rc == success && "Delete a record should not fail.");
            rids.erase(rids.begin() + index2);
        }


    }

    rbfm.closeFile(fileHandle);
    rc = rbfm.openFile(fileName, fileHandle);
    assert(rc == success && "Opening the file should not fail.");
    // check free space matches
    checkFreeSpaceMatch(fileHandle);
    // check whether exists empty space
    checkRecordOffsets(fileHandle);

    // delete remain records
    std::cout << "DELETING RECORDS" << std::endl;
    while (rids.size() != 0) {
        int index = engine() % rids.size();
        rc = rbfm.deleteRecord(fileHandle, largeRecordDescriptor, rids[index]);
        assert(rc == success && "Delete a record should not fail.");
        rids.erase(rids.begin() + index);
    }

    rbfm.closeFile(fileHandle);
    rc = rbfm.openFile(fileName, fileHandle);
    assert(rc == success && "Opening the file should not fail.");
    // check free space matches
    checkFreeSpaceMatch(fileHandle);
    // check whether exists empty space
    checkRecordOffsets(fileHandle);

    // check record number
    void *pageData = malloc(PAGE_SIZE);
    for (PageNum pageNum = 0; pageNum < fileHandle.getNumberOfPages(); pageNum++) {
        fileHandle.readPage(pageNum, pageData);
        DataPage page(pageData);
        assert(page.getRecordNumber() == 0 && "Record number should be 0");
    }

    std::cout << "File has no records, page number is: " << fileHandle.getNumberOfPages() << std::endl;

    free(record);
    free(pageData);
    free(nullsIndicator);
    free(largeNullsIndicator);
    rbfm.closeFile(fileHandle);

    std::cout << "RBF Test Custom Case 04 Finished! The result will not be examined :)" << std::endl << std::endl;

    remove(fileName.c_str());

    return 0;
}

int main() {
    // To test the functionality of the paged file manager
    RecordBasedFileManager &rbfm = RecordBasedFileManager::instance();

    // Remove files that might be created by previous test run
    remove("custom_test4");

    return RBFTest_Custom_4(rbfm);
}