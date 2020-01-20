#include <iostream>
#include <cmath>
#include "pfm.h"

PagedFileManager &PagedFileManager::instance() {
    static PagedFileManager _pf_manager = PagedFileManager();
    return _pf_manager;
}

PagedFileManager::PagedFileManager() = default;

PagedFileManager::~PagedFileManager() = default;

PagedFileManager::PagedFileManager(const PagedFileManager &) = default;

PagedFileManager &PagedFileManager::operator=(const PagedFileManager &) = default;

RC PagedFileManager::createFile(const std::string &fileName) {
    // std::cout << fileName.c_str() << std::endl; // debug
    FILE* file = fopen(fileName.c_str(), "rb");
    if(file != nullptr){
        fclose(file);
        // std::cout << "filename already exists" << std::endl; // debug
        return -1;
    }

    file = fopen(fileName.c_str(), "wb");
    if(file == nullptr){
        fclose(file);
        // std::cout << "Creation fail" << std::endl; // debug
        return -1;
    }

    fclose(file);
    return 0;
}

RC PagedFileManager::destroyFile(const std::string &fileName) {
    FILE* file = fopen(fileName.c_str(), "rb");
    if(file == nullptr){
        // std::cout << "File not exists" << std::endl; // debug
        return -1;
    }
    fclose(file);
    int r = remove(fileName.c_str());
    if(r != 0){
        // std::cout << "Deletion fails" << std::endl; // debug
        return -1;
    }

    return 0;
}

RC PagedFileManager::openFile(const std::string &fileName, FileHandle &fileHandle) {
    if (fileHandle.isOccupied()) return 1;
    FILE* file = fopen(fileName.c_str(), "rb");
    if(file == nullptr){
        fclose(file);
        // std::cout << "File not exists" << std::endl; // debug
        return -1;
    }
    fclose(file);
    std::fstream* f = new std::fstream;
    f->open(fileName, std::ios::in | std::ios::out | std::ios::binary);
    RC rc = fileHandle.setHandle(f);
    f = nullptr;
    delete(f);
    return rc;
}

RC PagedFileManager::closeFile(FileHandle &fileHandle) {
    return fileHandle.releaseHandle();
}

FileHandle::FileHandle() {
    readPageCounter = 0;
    writePageCounter = 0;
    appendPageCounter = 0;
    totalPageNum = 1;  // hidden page
    dataPageNum = 0;
    handle = nullptr;
    fspData = nullptr;
    curFSPNum = PAGE_SIZE;
}

FileHandle::~FileHandle() {
    delete(handle);
}

std::streampos FileHandle::getFileSize() noexcept {
    // TODO: add test
    std::streampos begin, end;
    handle->seekg(0, std::ios::beg);
    begin = handle->tellg();
    handle->seekg(0, std::ios::end);
    end = handle->tellg();
    return begin - end;
}

RC FileHandle::readHiddenPage(){
    void *data = malloc(PAGE_SIZE);
    if (data == nullptr) throw std::bad_alloc();
    RC rc = readPage(0, data, true);
    if (rc != 0) {
        free(data);
        return 1;
    }
    if (*((char *) data) != 'Y') {
        free(data);
        return 1;
    }
    int offset = sizeof(char);
    memcpy(&readPageCounter, (char *) data + offset, sizeof(Counter));
    offset += sizeof(Counter);
    memcpy(&writePageCounter, (char *) data + offset, sizeof(Counter));
    offset += sizeof(Counter);
    memcpy(&appendPageCounter, (char *) data + offset, sizeof(Counter));
    offset += sizeof(Counter);
    memcpy(&totalPageNum, (char *) data + offset, sizeof(PageNum));
    offset += sizeof(PageNum);
    memcpy(&dataPageNum, (char *) data + offset, sizeof(PageNum));
    free(data);
    return 0;
}

RC FileHandle::writeHiddenPage() {
    if (handle == nullptr) return 1;
    //void *data = malloc(sizeof(unsigned) * 3 + sizeof(char) + sizeof(PageNum));
    void *data = malloc(PAGE_SIZE);
    if (data == nullptr) throw std::bad_alloc();
    *((char *) data) = 'Y';
    int offset = sizeof(char);
    memcpy((char *) data + offset, &readPageCounter, sizeof(Counter));
    offset += sizeof(Counter);
    memcpy((char *) data + offset, &writePageCounter, sizeof(Counter));
    offset += sizeof(Counter);
    memcpy((char *) data + offset, &appendPageCounter, sizeof(Counter));
    offset += sizeof(Counter);
    memcpy((char *) data + offset, &totalPageNum, sizeof(PageNum));
    offset += sizeof(PageNum);
    memcpy((char *) data + offset, &dataPageNum, sizeof(PageNum));
    writePage(0, data, true);
    free(data);
    return 0;
}

RC FileHandle::setHandle(std::fstream *f) {
    if (handle != nullptr) {
        // a file is already opened
        return 1;
    }
    handle = f;
    // detect if this file was initialized
    if (readHiddenPage() != 0) {
        // write hidden page
        writeHiddenPage();
        // write first FSP
        void *fsp = malloc(PAGE_SIZE);
        memset(fsp, 0, PAGE_SIZE);
        appendPage(fsp, false);
        free(fsp);
    }
    return 0;
}

RC FileHandle::releaseHandle() {
    if (handle == nullptr) {
        // no file open
        return 1;
    } else {
        // write counters and total page number
        writePageCounter += 1;  // need to count the last write
        writeHiddenPage();
        handle->close();
        delete(handle);
        handle = nullptr;
        if (fspData != nullptr) free(fspData);
        return 0;
    }
}

bool FileHandle::isOccupied() {
    return handle != nullptr;
}

PageNum FileHandle::changeToActualPageNum(PageNum dataPageNum) {
    dataPageNum += 1;  // hidden page
    return ceil(float(dataPageNum) / PAGE_SIZE) + dataPageNum;
}

RC FileHandle::readPage(PageNum pageNum, void *data, bool actual) {
    if (!actual) {
        if (pageNum >= dataPageNum) return -1;
        pageNum = changeToActualPageNum(pageNum);
    } else {
        if (pageNum >= totalPageNum) return -1;
    }
    handle->clear();
    handle->seekg(pageNum * PAGE_SIZE, std::ios::beg);
    handle->read((char*) data, PAGE_SIZE);
    readPageCounter++;
    return 0;
}

RC FileHandle::writePage(PageNum pageNum, const void *data, bool actual) {
    if (!actual) {
        if (pageNum >= dataPageNum) return -1;
        pageNum = changeToActualPageNum(pageNum);
    } else {
        if (pageNum >= totalPageNum) return -1;
    }
    handle->clear();
    handle->seekp(pageNum * PAGE_SIZE, std::ios::beg);
    handle->write((const char*) data, PAGE_SIZE);
    writePageCounter++;
    return 0;
}

RC FileHandle::writePage(PageNum pageNum, const void *data, int freeSpace) {
    // update page's free space
    PageFreeSpacePercent percent = float(freeSpace) / PAGE_SIZE * 100;
    updateFreeSpacePercentOfPage(pageNum, percent);
    return writePage(pageNum, data);
}

RC FileHandle::appendPage(const void *data, bool dataPage) {
    if (dataPage) dataPageNum++;
    handle->seekp(totalPageNum * PAGE_SIZE, std::ios::beg);
    handle->write((const char *) data, PAGE_SIZE);
    appendPageCounter++;
    totalPageNum++;
    if (dataPage && dataPageNum % PAGE_SIZE == 0) {
        // need to insert a new FSP
        void *fsp = malloc(PAGE_SIZE);
        memset(fsp, 0, PAGE_SIZE);
        appendPage(fsp, false);
        free(fsp);
    }
    return 0;
}

RC FileHandle::appendPage(const void *data, int freeSpace) {
    RC rc = appendPage(data);
    // update page's free space
    PageFreeSpacePercent percent = float(freeSpace) / PAGE_SIZE * 100;
    updateFreeSpacePercentOfPage(dataPageNum - 1, percent);
    return rc;
}

int FileHandle::getNumberOfPages() {
    return dataPageNum;
}

int FileHandle::getActualNumberOfPages() {
    return totalPageNum;
}

RC FileHandle::collectCounterValues(unsigned &readPageCount, unsigned &writePageCount, unsigned &appendPageCount) {
    readPageCount = this->readPageCounter;
    writePageCount = this->writePageCounter;
    appendPageCount = this->appendPageCounter;
    return 0;
}

void FileHandle::getFSPofPage(const PageNum &pageNum, PageNum &fspNum, PageNum &pageIndex) {
    fspNum = (pageNum / PAGE_SIZE) * (PAGE_SIZE + 1) + 1;
    pageIndex = pageNum % PAGE_SIZE;
}

RC FileHandle::updateCurFSP(const PageNum &fspNum) {
    // load new fsp
    if (fspNum != curFSPNum) {
        if (fspData == nullptr) {
            // std::cout << "init fsp data " << std::endl; // debug
            fspData = malloc(PAGE_SIZE);
            if (fspData == nullptr) throw std::bad_alloc();
        }
        // std::cout << "load new fsp:" << fspNum << std::endl; // debug
        readPage(fspNum, fspData, true);
        curFSP.loadNewPage(fspData);
        curFSPNum = fspNum;
        return 0;
    } else {
        return -1;
    }
}

PageFreeSpacePercent FileHandle::getFreeSpacePercentOfPage(const PageNum pageNum) {
    // get fsp page number and pageIndex in that fsp and update current fsp member
    assert(pageNum < dataPageNum);
    PageNum fspNum, pageIndex;
    getFSPofPage(pageNum, fspNum, pageIndex);
    updateCurFSP(fspNum);
    return curFSP.getFreeSpace(pageIndex);
}

void FileHandle::updateFreeSpacePercentOfPage(const PageNum pageNum, const PageFreeSpacePercent freePageSpace) {
    PageNum fspNum, pageIndex;
    getFSPofPage(pageNum, fspNum, pageIndex);
    updateCurFSP(fspNum);
    curFSP.writeFreeSpace(pageIndex, freePageSpace);
}

// ========================================================================================
//                                 Free Space Page Class
// ========================================================================================

FreeSpacePage::FreeSpacePage(void *data) {
    assert(data != nullptr);
    page = data;
}

void FreeSpacePage::loadNewPage(void *data) {
    assert(data != nullptr);
    page = data;
}

PageFreeSpacePercent FreeSpacePage::getFreeSpace(PageNum pageIndex) {
    assert(pageIndex < PAGE_SIZE);
    PageFreeSpacePercent freePageSpace;
    memcpy(&freePageSpace, (char *) page + pageIndex, 1);
    return freePageSpace;
}

void FreeSpacePage::writeFreeSpace(PageNum pageIndex, PageFreeSpacePercent freePageSpace) {
    assert(pageIndex < PAGE_SIZE);
    assert(freePageSpace <= 100);
    memcpy((char *) page + pageIndex, &freePageSpace, 1);
}
