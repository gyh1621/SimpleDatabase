#include <fstream>

#include "ix.h"

IndexManager &IndexManager::instance() {
    static IndexManager _index_manager = IndexManager();
    return _index_manager;
}

bool IndexManager::fileExist(const std::string &fileName) {
    std::ifstream f(fileName.c_str());
    return f.good();
}

RC IndexManager::createFile(const std::string &fileName) {
    if (fileExist(fileName)) {
        return 1;
    }
    std::ofstream file(fileName);
    return 0;
}

RC IndexManager::destroyFile(const std::string &fileName) {
    if (!fileExist(fileName)) {
        return 1;
    }
    return remove(fileName.c_str());
}

RC IndexManager::openFile(const std::string &fileName, IXFileHandle &ixFileHandle) {
    if (ixFileHandle.isOccupied()) return -1;
    if (!fileExist(fileName)) return 1;
    auto* f = new std::fstream;
    f->open(fileName, std::ios::in | std::ios::out | std::ios::binary);
    ixFileHandle.setHandle(f);
    f = nullptr;
    delete(f);
    return 0;
}

RC IndexManager::closeFile(IXFileHandle &ixFileHandle) {
    ixFileHandle.releaseHandle();
    return 0;
}

RC IndexManager::insertEntry(IXFileHandle &ixFileHandle, const Attribute &attribute, const void *key, const RID &rid) {
    return -1;
}

RC IndexManager::deleteEntry(IXFileHandle &ixFileHandle, const Attribute &attribute, const void *key, const RID &rid) {
    return -1;
}

RC IndexManager::scan(IXFileHandle &ixFileHandle,
                      const Attribute &attribute,
                      const void *lowKey,
                      const void *highKey,
                      bool lowKeyInclusive,
                      bool highKeyInclusive,
                      IX_ScanIterator &ix_ScanIterator) {
    return -1;
}

void IndexManager::printBtree(IXFileHandle &ixFileHandle, const Attribute &attribute) const {
}

IX_ScanIterator::IX_ScanIterator() {
}

IX_ScanIterator::~IX_ScanIterator() {
}

RC IX_ScanIterator::getNextEntry(RID &rid, void *key) {
    return -1;
}

RC IX_ScanIterator::close() {
    return -1;
}

IXFileHandle::IXFileHandle() {
    ixReadPageCounter = 0;
    ixWritePageCounter = 0;
    ixAppendPageCounter = 0;
    totalPageNum = 1;  // hidden page
    handle = nullptr;
    rootPageID = 0;
}

RC IXFileHandle::writeHiddenPage() {
    if (handle == nullptr) return 1;
    void *data = malloc(PAGE_SIZE);
    if (data == nullptr) throw std::bad_alloc();
    memset(data, 0, PAGE_SIZE);
    *((char *) data) = 'Y';
    PageOffset offset = sizeof(char);
    ixWritePageCounter++;  // count this write
    memcpy((char *) data + offset, &ixReadPageCounter, sizeof(Counter));
    offset += sizeof(Counter);
    memcpy((char *) data + offset, &ixWritePageCounter, sizeof(Counter));
    offset += sizeof(Counter);
    memcpy((char *) data + offset, &ixAppendPageCounter, sizeof(Counter));
    offset += sizeof(Counter);
    memcpy((char *) data + offset, &totalPageNum, sizeof(PageNum));
    offset += sizeof(PageNum);
    memcpy((char *) data + offset, &rootPageID, sizeof(PageNum));
    writeNodePage(data, 0);
    free(data);
    return 0;
}

RC IXFileHandle::readHiddenPage() {
    void *data = malloc(PAGE_SIZE);
    if (data == nullptr) throw std::bad_alloc();
    RC rc = readNodePage(data, 0);
    if (*((char *) data) != 'Y') {
        free(data);
        return 1;
    }
    PageOffset offset = sizeof(char);
    memcpy(&ixReadPageCounter, (char *) data + offset, sizeof(Counter));
    offset += sizeof(Counter);
    memcpy(&ixWritePageCounter, (char *) data + offset, sizeof(Counter));
    offset += sizeof(Counter);
    memcpy(&ixAppendPageCounter, (char *) data + offset, sizeof(Counter));
    offset += sizeof(Counter);
    memcpy(&totalPageNum, (char *) data + offset, sizeof(PageNum));
    offset += sizeof(PageNum);
    memcpy(&rootPageID, (char *) data + offset, sizeof(PageNum));
    free(data);
    ixReadPageCounter++;  // count this read
    return 0;
}

RC IXFileHandle::appendNodePage(const void *pageData, PageNum &pageID) {
    handle->seekp(totalPageNum * PAGE_SIZE, std::ios::beg);
    handle->write((const char *) pageData, PAGE_SIZE);
    ixAppendPageCounter++;
    pageID = totalPageNum;
    totalPageNum++;
    return 0;
}

RC IXFileHandle::readNodePage(void *pageData, const PageNum &pageID) {
    if (pageID >= totalPageNum) return -1;
    handle->clear();
    handle->seekg(pageID* PAGE_SIZE, std::ios::beg);
    handle->read((char*) pageData, PAGE_SIZE);
    ixReadPageCounter++;
    return 0;
}

RC IXFileHandle::writeNodePage(const void *pageData, const PageNum &pageID) {
    if (pageID >= totalPageNum) return -1;
    handle->clear();
    handle->seekp(pageID * PAGE_SIZE, std::ios::beg);
    handle->write((const char*) pageData, PAGE_SIZE);
    ixWritePageCounter++;
    return 0;
}

bool IXFileHandle::isOccupied() {
    return handle != nullptr;
}

void IXFileHandle::setHandle(std::fstream *f) {
    handle = f;
    // detect if this file was initialized
    RC rc = readHiddenPage();
}

void IXFileHandle::releaseHandle() {
    writeHiddenPage();
    handle->close();
    delete(handle);
    handle = nullptr;
}

RC IXFileHandle::collectCounterValues(Counter &readPageCount, Counter &writePageCount, Counter &appendPageCount) {
    readPageCount = ixReadPageCounter;
    writePageCount = ixWritePageCounter;
    appendPageCount = ixAppendPageCounter;
    return 0;
}

IXFileHandle::~IXFileHandle() {
    delete(handle);
}

