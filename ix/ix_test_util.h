#ifndef _ix_test_util_h_
#define _ix_test_util_h_

#ifndef _fail_
#define _fail_
const int fail = -1;
#endif

#include "../rbf/test_util.h"

using namespace std;

IndexManager &indexManager = IndexManager::instance();

void createFileShouldNotFail(const std::string &indexFileName) {
    RC rc = indexManager.createFile(indexFileName);
    assert(rc == success && "indexManager::createFile() should not fail.");
}

void openFileShouldNotFail(const std::string &indexFileName, IXFileHandle &ixFileHandle) {
    RC rc = indexManager.openFile(indexFileName, ixFileHandle);
    assert(rc == success && "indexManager::openFile() should not fail.");
}

void closeFileShouldNotFail(const std::string &indexFileName, IXFileHandle &ixFileHandle) {
    RC rc = indexManager.closeFile(ixFileHandle);
    assert(rc == success && "indexManager::closeFile() should not fail.");
    assert(!ixFileHandle.isOccupied() && "ix file handle should not be occupied.");
}

void appendPageShouldNotFail(const void *pageData, IXFileHandle &ixFileHandle, PageNum &pageID) {
    RC rc = ixFileHandle.appendNodePage(pageData, pageID);
    assert(rc == success && "append page should not fail.");
}

void readPageShouldNotFail(void *pageData, IXFileHandle &ixFileHandle, const PageNum &pageID) {
    RC rc = ixFileHandle.readNodePage(pageData, pageID);
    assert(rc == success && "read page should not fail.");
}

void writePageShouldNotFail(const void *pageData, IXFileHandle &ixFileHandle, const PageNum &pageID) {
    RC rc = ixFileHandle.writeNodePage(pageData, pageID);
    assert(rc == success && "write page should not fail.");
}

#endif


