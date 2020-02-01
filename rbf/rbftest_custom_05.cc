#define private public
#include "rbfm.h"
#include "test_util.h"

int RBFTest_Custom_5(RecordBasedFileManager &rbfm) {
    // Functions Tested:
    // Construct Record Object from formatted data

    std::cout << std::endl << "***** In RBF Test Custom Case 05 *****" << std::endl;

    RC rc;
    std::string fileName = "custom_test5";

    // Create files named "custom_test5"
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
    void *pageData = malloc(PAGE_SIZE);
    memset(pageData, 0, PAGE_SIZE);
    int numRecords = 1000;

    std::vector <Attribute> recordDescriptor;
    createLargeRecordDescriptor2(recordDescriptor);

    // NULL field indicator
    int nullFieldsIndicatorActualSize = getActualByteForNullsIndicator(recordDescriptor.size());
    auto *nullsIndicator = (unsigned char *) malloc(nullFieldsIndicatorActualSize);
    memset(nullsIndicator, 0, nullFieldsIndicatorActualSize);

    for (int i = 0; i < numRecords; i++) {
        // inserting Record
        memset(record, 0, 1000);
        int size = 0;
        prepareLargeRecord2(recordDescriptor.size(), nullsIndicator, i, record, &size);
        rc = rbfm.insertRecord(fileHandle, recordDescriptor, record, rid);
        assert(rc == success && "Inserting a record should not fail.");
        // construct Record from descriptor and raw data
        Record tmpRecord(recordDescriptor, record);
        // construct Record from formatted data
        void *recordData = malloc(tmpRecord.size);
        memcpy(recordData, tmpRecord.getRecordData(), tmpRecord.size);
        Record tmpRecord2(recordData);
        assert(tmpRecord.size == tmpRecord2.size && "Record size should equal.");
        assert(tmpRecord.fieldNumber == tmpRecord2.fieldNumber && "Record field number should equal.");
        free(recordData);
    }

    free(pageData);
    free(record);
    free(nullsIndicator);
    rbfm.closeFile(fileHandle);

    std::cout << "RBF Test Custom Case 05 Finished! The result will not be examined :)" << std::endl << std::endl;

    return 0;
}

int main() {
    // To test the functionality of the paged file manager
    RecordBasedFileManager &rbfm = RecordBasedFileManager::instance();

    // Remove files that might be created by previous test run
    remove("custom_test5");

    return RBFTest_Custom_5(rbfm);
}
