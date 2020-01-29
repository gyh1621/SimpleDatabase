#include "rbfm.h"
#include <iostream>

RecordBasedFileManager &RecordBasedFileManager::instance() {
    static RecordBasedFileManager _rbf_manager = RecordBasedFileManager();
    return _rbf_manager;
}

RecordBasedFileManager::RecordBasedFileManager() = default;

RecordBasedFileManager::~RecordBasedFileManager() = default;

RecordBasedFileManager::RecordBasedFileManager(const RecordBasedFileManager &) = default;

RecordBasedFileManager &RecordBasedFileManager::operator=(const RecordBasedFileManager &) = default;

RC RecordBasedFileManager::getFirstPageAvailable(FileHandle &fileHandle, const int &freeSize, PageNum &pageNum) {

    int totalPage = fileHandle.getNumberOfPages(), curPage = 0;

    // find a page large enough
    PageFreeSpace pageFreeSpace;
    while (curPage < totalPage) {
        pageFreeSpace = fileHandle.getFreeSpaceOfPage(curPage);
        if (pageFreeSpace >= freeSize) {
            pageNum = curPage;
            return 0;
        }
        curPage++;
    }

    return -1;
}

RC RecordBasedFileManager::createFile(const std::string &fileName) {
    return PagedFileManager::instance().createFile(fileName);
}

RC RecordBasedFileManager::destroyFile(const std::string &fileName) {
    return PagedFileManager::instance().destroyFile(fileName);
}

RC RecordBasedFileManager::openFile(const std::string &fileName, FileHandle &fileHandle) {
    return PagedFileManager::instance().openFile(fileName, fileHandle);
}

RC RecordBasedFileManager::closeFile(FileHandle &fileHandle) {
    return PagedFileManager::instance().closeFile(fileHandle);
}

RC RecordBasedFileManager::insertRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                        const void *data, RID &rid) {
    Record record(recordDescriptor, data);

    // try to find a page available
    PageNum pageNum;
    int slotID;
    void *pageData = malloc(PAGE_SIZE);
    auto neededSize = record.getSize() + DataPage::SlotSize;
    RC rc = getFirstPageAvailable(fileHandle, neededSize, pageNum);

    if (rc != 0) {
        // fail to find a page available, create a new page
        //std::cout << "create new page to insert needed size " << neededSize << std::endl;  //debug
        DataPage page(pageData);
        slotID = page.insertRecord(record);
        // write to file
        fileHandle.appendPage(page.getFreeSpace(), page.getPageData());
        pageNum = fileHandle.getNumberOfPages() - 1;
    } else {
        // find a page available, load page data
        fileHandle.readPage(pageNum, pageData);
        DataPage page(pageData);
        //std::cout << "fina a page to insert needed size " << neededSize << " free space " << page.getFreeSpace() << std::endl; // debug
        slotID = page.insertRecord(record);
        // write to file
        fileHandle.writePage(pageNum, page.getFreeSpace(), page.getPageData());
    }
    //std::cout << "PAGE: " << pageNum << std::endl;  // debug

    rid.pageNum = pageNum;
    rid.slotNum = slotID;

    memset(pageData, 0, PAGE_SIZE);
    free(pageData);

    return 0;
}

RC RecordBasedFileManager::readRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                      const RID &rid, void *data) {
    auto pageid = rid.pageNum;
    auto slotid = rid.slotNum;
    void* targetPage = malloc(PAGE_SIZE);
    bool isPointer = true;
    RC success = 1;

    while(isPointer){
        fileHandle.readPage(pageid, targetPage);
        DataPage p(targetPage);
        success = p.readRecordIntoRaw(recordDescriptor, data, isPointer, pageid, slotid);
    }
    //std::cout << "PAGE " << pageid << " SLOT " << slotid << std::endl;  // debug
    free(targetPage);
    return success;
}

RC RecordBasedFileManager::deleteRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                        const RID &rid) {
    return -1;
}

RC RecordBasedFileManager::printRecord(const std::vector<Attribute> &recordDescriptor, const void *data) {
    Record r(recordDescriptor, data);
    r.printRecord(recordDescriptor);
    return 0;
}

RC RecordBasedFileManager::updateRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                        const void *data, const RID &rid) {
    return -1;
}

RC RecordBasedFileManager::readAttribute(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                         const RID &rid, const std::string &attributeName, void *data) {
    return -1;
}

RC RecordBasedFileManager::scan(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                const std::string &conditionAttribute, const CompOp compOp, const void *value,
                                const std::vector<std::string> &attributeNames, RBFM_ScanIterator &rbfm_ScanIterator) {
    return -1;
}

