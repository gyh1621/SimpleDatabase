//
// Created by 宋立杰 on 2020/1/16.
//

#include "rbfm.h"
#include "test_util.h"

int RBFTest_Custom_2(RecordBasedFileManager &rbfm) {
    // Functions Tested:
    // 1. test make a new page with correct format
    // 2. test creating a Record object
    // 3. test insert multiple records to a page;
    // 4. test print multiple records
    std::cout << std::endl << "***** In RBF Test Custom Case 02 *****" << std::endl;

    RC rc;
    std::string fileName = "custom_test_2";

    // create file for custom test 2;
    rc = rbfm.createFile(fileName);
    assert(rc == success && "Creating file should not fail");

    rc = createFileShouldSucceed(fileName);
    assert(rc == success && "Creating file should not fail");

    // init fileHandle and open file;
    FileHandle fileHandle;
    rc = rbfm.openFile(fileName, fileHandle);
    assert(rc == success && "Opening file should not fail");

    // prepare multiple records and insert to pages;
    int rcSize = 0;
    RID rid1, rid2, rid3;
    void* record1 = malloc(100);
    void* returnedData1 = malloc(100);
    void* record2 = malloc(100);
    void* returnedData2 = malloc(100);
    void* record3 = malloc(100);
    void* returnedData3 = malloc(100);
    std::vector<Attribute> recordDescriptor;
    createRecordDescriptor(recordDescriptor);
    int nullFieldIndicatorActualSize = getActualByteForNullsIndicator(recordDescriptor.size());
    auto *nullIndicator = (unsigned char *) malloc(nullFieldIndicatorActualSize);

    //insert record1
    memset(nullIndicator, 0, nullFieldIndicatorActualSize);
    prepareRecord(recordDescriptor.size(), nullIndicator, 8, "asdfghjk", 19, 169.6, 5200, record1, &rcSize);
    std::cout << "Record1 to insert:" << std::endl;
    rbfm.printRecord(recordDescriptor, record1);
    rc = rbfm.insertRecord(fileHandle, recordDescriptor, record1, rid1);
    assert(rc == success && "Insert Record1 failed.");

    // insert record2
    memset(nullIndicator, 0, nullFieldIndicatorActualSize);
    prepareRecord(recordDescriptor.size(), nullIndicator, 6, "qwerty", 22, 172.3, 3456, record2, &rcSize);
    std::cout << "Record2 to insert:" << std::endl;
    rbfm.printRecord(recordDescriptor, record2);
    rc = rbfm.insertRecord(fileHandle, recordDescriptor, record2, rid2);
    assert(rc == success && "Insert Record2 failed.");

    // insert record3
    memset(nullIndicator, 0, nullFieldIndicatorActualSize);
    prepareRecord(recordDescriptor.size(), nullIndicator, 10, "zxcvbnmlpo", 21, 180.2, 9992, record3, &rcSize);
    std::cout << "Record3 to insert:" << std::endl;
    rbfm.printRecord(recordDescriptor, record3);
    rc = rbfm.insertRecord(fileHandle, recordDescriptor, record3, rid3);
    assert(rc == success && "Insert Record3 failed.");

    // read record1 inserted above
    rc = rbfm.readRecord(fileHandle, recordDescriptor, rid1, returnedData1);
    assert(rc == success && "Reading record1 failed");
    //print record1
    std::cout << "Record1: " << std::endl;
    rbfm.printRecord(recordDescriptor, returnedData1);

    // read record2 inserted above
    rc = rbfm.readRecord(fileHandle, recordDescriptor, rid2, returnedData2);
    assert(rc == success && "Reading record2 failed");
    //print record2
    std::cout << "Record2: " << std::endl;
    rbfm.printRecord(recordDescriptor, returnedData2);

    // read record3 inserted above
    rc = rbfm.readRecord(fileHandle, recordDescriptor, rid3, returnedData3);
    assert(rc == success && "Reading record3 failed");
    //print record3
    std::cout << "Record3: " << std::endl;
    rbfm.printRecord(recordDescriptor, returnedData3);

    // prepare records with null to insert
    std::cout << std::endl << "Testing records with null fields" << std::endl << std::endl;
    void *data = malloc(100);
    void *returnedData = malloc(100);
    RID rid;
    int fieldNum = recordDescriptor.size();
    memset(nullIndicator, 1, nullFieldIndicatorActualSize);
    for (int i = 0; i <= fieldNum; i++) {
        nullIndicator[0] |= (unsigned) 1 << (8 - i);
        prepareRecord(fieldNum, nullIndicator, 5,"aname", 100, 200, 300, data, &rcSize);
        std::cout << "Record to insert:\t";
        rbfm.printRecord(recordDescriptor, data);

        rc = rbfm.insertRecord(fileHandle, recordDescriptor, data, rid);
        assert(rc == success && "Insert record failed.");

        rc = rbfm.readRecord(fileHandle, recordDescriptor, rid, returnedData);
        assert(rc == success && "Reading record2 failed");
        std::cout << "Return data:\t\t";
        rbfm.printRecord(recordDescriptor, returnedData);
        std::cout << std::endl;
    }
    free(data);
    free(returnedData);

    // close file  "custom_test_2"
    rc = rbfm.closeFile(fileHandle);
    assert(rc == success && "Closing file failed");

    // destroy file
    rc = rbfm.destroyFile(fileName);
    assert(rc == success && "Destroying file failed");

    rc = destroyFileShouldSucceed(fileName);
    assert(rc == success && "Destroying file failed");
    free(nullIndicator);
    free(record1);
    free(returnedData1);
    free(record2);
    free(returnedData2);
    free(record3);
    free(returnedData3);

//    // build a new page
//    void* data = malloc(PAGE_SIZE);
//    if (data == nullptr) throw std::bad_alloc();
//    rc = rbfm.makePage(data);
//    assert(rc == success && "Init page should not fail");
//
//    // check new page info data
//    int offset = PAGE_SIZE - sizeof(unsigned) * 2;
//    unsigned slotNum = *((unsigned * )((char*) data + offset));
//    offset += sizeof(unsigned);
//    unsigned freeBytes = *((unsigned * )((char*) data + offset));
//    assert(slotNum == 0 && "slotNum should be 0");
//    assert(freeBytes == PAGE_SIZE - sizeof(unsigned) * 2 && "free bytes should be 4088");
//    free(data);


    std::cout << "RBF Test Custom Case 02 Finished! The result will not be examined :)" << std::endl;
    return 0;
}

int main() {
    // To test the functionality of the record based file manager
    RecordBasedFileManager &rbfm = RecordBasedFileManager::instance();

    remove("custom_test_2");

    return RBFTest_Custom_2(rbfm);
}

