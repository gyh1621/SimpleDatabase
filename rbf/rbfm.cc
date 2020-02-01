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

//void RecordBasedFileManager::findDataPage(FileHandle &fileHandle, RecordLength &pageid, RecordOffset &slotid,
//                                          void *targetPage) {
//    bool isPointer = true;
//    while(isPointer){
//        fileHandle.readPage(pageid,targetPage);
//        DataPage p(targetPage);
//        p.checkRecordExist(isPointer, pageid, slotid);
//    }
//}

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

    if (rc == -1) {
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
    void* targetPage = malloc(PAGE_SIZE);
    if (targetPage == nullptr) throw std::bad_alloc();
    RC rc;

    RID actualRID;
    actualRID.slotNum = rid.slotNum;
    actualRID.pageNum = rid.pageNum;
    rc = findRecordActualRID(fileHandle, actualRID);
    if (rc == 1) {
        free(targetPage);
        return rc;  // deleted record
    }

    fileHandle.readPage(actualRID.pageNum, targetPage);
    DataPage p(targetPage);
    p.readRecordIntoRaw(actualRID.slotNum, recordDescriptor, data);

    //std::cout << "PAGE " << pageid << " SLOT " << slotid << std::endl;  // debug
    free(targetPage);
    return rc;
}

RC RecordBasedFileManager::deleteRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                        const RID &rid) {
    void* targetPage = malloc(PAGE_SIZE);
    RC rc;

    RID actualRID;
    actualRID.pageNum = rid.pageNum;
    actualRID.slotNum = rid.slotNum;
    rc = findRecordActualRID(fileHandle, actualRID, true);

    fileHandle.readPage(actualRID.pageNum, targetPage);
    DataPage p(targetPage);
    p.deleteRecord(actualRID.slotNum);
    fileHandle.writePage(actualRID.pageNum, p.getFreeSpace(), p.getPageData());

    free(targetPage);
    return rc;
}

RC RecordBasedFileManager::printRecord(const std::vector<Attribute> &recordDescriptor, const void *data) {
    Record r(recordDescriptor, data);
    r.printRecord(recordDescriptor);
    return 0;
}

RC RecordBasedFileManager::updateRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                        const void *data, const RID &rid) {
    Record updatedRecord(recordDescriptor, data);
    void* targetPage = malloc(PAGE_SIZE);
    if (targetPage == nullptr) throw std::bad_alloc();
    memset(targetPage, 0, PAGE_SIZE);

    RC rc;
    RID actualRID;
    actualRID.pageNum = rid.pageNum;
    actualRID.slotNum = rid.slotNum;
    rc = findRecordActualRID(fileHandle, actualRID);
    if (rc == 1) return rc;  // deleted record

    fileHandle.readPage(actualRID.pageNum, targetPage);
    DataPage page(targetPage);

    RecordLength oldRecordSize = page.getRecordSize(actualRID.slotNum);
    auto freespace = page.getFreeSpace();
    auto updatedSize = updatedRecord.getSize();

    if (oldRecordSize >= updatedSize || updatedSize - oldRecordSize <= freespace) {
        // can fit into current page
        page.updateRecord(updatedRecord, actualRID.slotNum);
    } else {
        // insert to new page
        RID newRID;
        insertRecord(fileHandle, recordDescriptor, data, newRID);
        // remove record from this page and turn the slot into a pointer
        page.moveRecord(actualRID.slotNum, newRID);
    }

    fileHandle.writePage(actualRID.pageNum, page.getFreeSpace(), page.getPageData());

    free(targetPage);
    return 0;
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

int RecordBasedFileManager::findRecordActualRID(FileHandle &fileHandle, RID &rid, bool deletePointer) {
    int result = -1;
    void *pageData = malloc(PAGE_SIZE);
    if (pageData == nullptr) throw std::bad_alloc();
    while (result != 0) {
        SlotNumber slot = rid.slotNum;
        fileHandle.readPage(rid.pageNum, pageData);
        DataPage page(pageData);
        result = page.checkRecordExist(slot, rid);
        if (result == 1) {
            // deleted record
            break;
        } else if (result == -1) {
            // pointer
            if (deletePointer) page.deleteSlot(slot);
        }
    }
    free(pageData);
    return result;
}
