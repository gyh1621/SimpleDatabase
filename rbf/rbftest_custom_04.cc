#define private public

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
    for (int i = 0; i < numRecords; i++) {
        // Test insert Record
        memset(record, 0, 1000);
        int size = 0;
        if (i % 2 == 0) {
            prepareLargeRecord2(largeRecordDescriptor.size(), largeNullsIndicator, i, record, &size);
            rc = rbfm.insertRecord(fileHandle, largeRecordDescriptor, record, rid);
        } else {
            prepareRecord(recordDescriptor.size(), nullsIndicator, 8, "ressssss", 10, 10, 10, record, &size);
            rc = rbfm.insertRecord(fileHandle, recordDescriptor, record, rid);
        }
        assert(rc == success && "Inserting a record should not fail.");
    }

    rbfm.closeFile(fileHandle);
    rc = rbfm.openFile(fileName, fileHandle);
    assert(rc == success && "Opening the file should not fail.");

    // check free space matches
    void *pageData = malloc(PAGE_SIZE);
    for (PageNum i = 0; i < fileHandle.getNumberOfPages(); i++) {
        fileHandle.readPage(i, pageData);
        DataPage page(pageData);
        PageFreeSpace space1 = page.getFreeSpace(), space2 = fileHandle.getFreeSpaceOfPage(i);
        if (i % 200 == 0) std::cout << "Data page " << i << " fs computed: " << space1 << " fs recorded: " << space2 << std::endl;
        assert(page.getFreeSpace() == fileHandle.getFreeSpaceOfPage(i) && "free space should match");
    }
    std::cout << "All free spaces matches." << std::endl << std::endl;

    // check whether exists empty space
    bool isPointer = true;
    unsigned recordOffset;
    unsigned short recordLen;
    unsigned lastRecordEnd = 0;
    for (PageNum i = 0; i < fileHandle.getNumberOfPages(); i++) {
        fileHandle.readPage(i, pageData);
        DataPage page(pageData);
        lastRecordEnd = 0;
        for (unsigned slot = 0; slot < page.slotNumber; slot++) {
            while (slot < page.slotNumber) {
                // read until next non-pointer slot
                page.parseSlot(slot, isPointer, recordOffset, recordLen);
                if (isPointer) slot++;
                else break;
            }
            if (!isPointer) {
                assert(lastRecordEnd == recordOffset && "record offset should match");
                lastRecordEnd = recordOffset + recordLen;
            } else {
                // slot == page.slotNumber
                break;
            }
        }
    }
    std::cout << "No empty spaces between records" << std::endl << std::endl;

    free(record);
    free(nullsIndicator);
    free(largeNullsIndicator);
    free(pageData);
    rbfm.closeFile(fileHandle);

    std::cout << "RBF Test Custom Case 04 Finished! The result will not be examined :)" << std::endl << std::endl;

    return 0;
}

int main() {
    // To test the functionality of the paged file manager
    RecordBasedFileManager &rbfm = RecordBasedFileManager::instance();

    // Remove files that might be created by previous test run
    remove("custom_test4");

    return RBFTest_Custom_4(rbfm);
}