#include <cstring>
#define private public
#include "ix.h"
#include "ix_test_util.h"


RC customTestCase_1(const std::string &indexFileName) {
    // Functions tested
    // r/w/a of create file, close file, reopen file
    std::cout << std::endl << "***** In IX Custom Test Case 01 *****" << std::endl;

    Counter readCounter, writeCounter, appendCounter;

    // create and open index file
    createFileShouldNotFail(indexFileName);
    IXFileHandle ixFileHandle;
    openFileShouldNotFail(indexFileName, ixFileHandle);

    // collect counters, r/w/a should be 1/1/0
    // one read is trying to read the hidden page and one write of writing hidden file
    ixFileHandle.collectCounterValues(readCounter, writeCounter, appendCounter);
    if (!(readCounter == 1 && writeCounter == 1 && appendCounter == 0)) {
        std::cout << "counters after opening a newly created file are not correct, r/w/a: "
                  << readCounter << " " << writeCounter << " " << appendCounter;
        return 1;
    }

    // close and reopen index file
    closeFileShouldNotFail(indexFileName, ixFileHandle);
    openFileShouldNotFail(indexFileName, ixFileHandle);

    // collect counters, r/w/a should be 2/2/0
    // one extra of reading hidden page when opening, one extra write should be writing hidden page when closing
    ixFileHandle.collectCounterValues(readCounter, writeCounter, appendCounter);
    if (!(readCounter == 2 && writeCounter == 2 && appendCounter == 0)) {
        std::cout << "counters after reopening file are not correct, r/w/a: "
                  << readCounter << " " << writeCounter << " " << appendCounter;
        return 1;
    }

    // append 1000 page
    PageNum appendPageNum = 1000;
    PageNum pageID;
    PageNum writePageNum = 0;
    for (PageNum i = 0; i < appendPageNum; i++) {
        void *data = malloc(PAGE_SIZE);
        memset(data, 0, PAGE_SIZE);
        for (int j = 0; j < PAGE_SIZE / sizeof(int); j+=sizeof(int)) {
            int n = j + i;
            memcpy((char *) data + j, &n, sizeof(int));
        }
        appendPageShouldNotFail(data, ixFileHandle, pageID);
        assert(pageID == 1 + i);
        if (i % 100 == 0) {
            memset(data, 0, PAGE_SIZE);
            for (int j = 0; j < PAGE_SIZE / sizeof(int); j+=sizeof(int)) {
                int n = j + i + 100;
                memcpy((char *) data + j, &n, sizeof(int));
            }
            writePageShouldNotFail(data, ixFileHandle, pageID);
            writePageNum++;
        }
        free(data);
    }
    assert(ixFileHandle.totalPageNum == appendPageNum + 1);

    // close and reopen file
    closeFileShouldNotFail(indexFileName, ixFileHandle);
    openFileShouldNotFail(indexFileName, ixFileHandle);

    // collect counters, r/w/a should be 3/3+writePageNum/1000
    ixFileHandle.collectCounterValues(readCounter, writeCounter, appendCounter);
    if (!(readCounter == 3 && writeCounter == 3+writePageNum && appendCounter == 1000)) {
        std::cout << "counters after appending are not correct, r/w/a: "
                  << readCounter << " " << writeCounter << " " << appendCounter;
        return 1;
    }

    // compare append page
    for (PageNum i = 0; i < appendPageNum; i++) {
        void *data = malloc(PAGE_SIZE);
        readPageShouldNotFail(data, ixFileHandle, i + 1);
        void *cmpData = malloc(PAGE_SIZE);
        memset(cmpData, 0, PAGE_SIZE);
        if (i % 100 == 0) {
            for (int j = 0; j < PAGE_SIZE / sizeof(int); j+=sizeof(int)) {
                int n = j + i + 100;
                memcpy((char *) cmpData + j, &n, sizeof(int));
            }
        } else {
            for (int j = 0; j < PAGE_SIZE / sizeof(int); j+=sizeof(int)) {
                int n = j + i;
                memcpy((char *) cmpData + j, &n, sizeof(int));
            }
        }
        assert(memcmp(data, cmpData, PAGE_SIZE) == 0);
        free(data);
        free(cmpData);
    }

    closeFileShouldNotFail(indexFileName, ixFileHandle);

    return success;
}

int main() {

    const std::string indexFileName = "custom_test_idx";
    RC rc = indexManager.destroyFile("custom_test_idx");

    if (customTestCase_1(indexFileName) == success) {
        std::cout << "***** IX Test Custom Case 01 finished. *****" << std::endl;
        return success;
    } else {
        std::cout << "***** [FAIL] IX Custom Test Case 01 failed. *****" << std::endl;
        return fail;
    }
}
