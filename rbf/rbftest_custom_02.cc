//
// Created by 宋立杰 on 2020/1/16.
//

#include "rbfm.h"
#include "test_util.h"

int RBFTest_Custom_2(RecordBasedFileManager &rbfm) {
    // Functions Tested:
    // 1. test make a new page with correct format
    // 2. test creating a Record object
    std::cout << std::endl << "***** In RBF Test Custom Case 02 *****" << std::endl;

    RC rc;

    // build a new page
    void* data = malloc(PAGE_SIZE);
    if (data == nullptr) throw std::bad_alloc();
    rc = rbfm.makePage(data);
    assert(rc == success && "Init page should not fail");

    // check new page info data
    int offset = PAGE_SIZE - sizeof(unsigned) * 2;
    unsigned slotNum = *((unsigned * )((char*) data + offset));
    offset += sizeof(unsigned);
    unsigned freeBytes = *((unsigned * )((char*) data + offset));
    assert(slotNum == 0 && "slotNum should be 0");
    assert(freeBytes == PAGE_SIZE - sizeof(unsigned) * 2 && "free bytes should be 4088");

    free(data);

    // prepare a new record
    void *record = malloc(100);
    int recordSize = 0;
    std::vector<Attribute> recordDescriptor;
    createRecordDescriptor(recordDescriptor);
    int nullFieldsIndicatorActualSize = getActualByteForNullsIndicator(recordDescriptor.size());
    auto *nullsIndicator = (unsigned char *) malloc(nullFieldsIndicatorActualSize);
    memset(nullsIndicator, 0, nullFieldsIndicatorActualSize);
    prepareRecord(recordDescriptor.size(), nullsIndicator, 8, "Anteater", 25, 177.8, 6200, record, &recordSize);

    Record newRecord(recordDescriptor, record);


    std::cout << "RBF Test Custom Case 02 Finished! The result will not be examined :)." << std::endl;
    return 0;
}

int main() {
    // To test the functionality of the record based file manager
    RecordBasedFileManager &rbfm = RecordBasedFileManager::instance();

    return RBFTest_Custom_2(rbfm);
}

