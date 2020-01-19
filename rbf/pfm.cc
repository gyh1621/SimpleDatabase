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
    // TODO: fix add test
    std::cout << fileName.c_str() << std::endl;
    FILE* file = fopen(fileName.c_str(), "rb");
    if(file != nullptr){
        fclose(file);
        std::cout << "filename already exists" << std::endl;
        return -1;
    }

    file = fopen(fileName.c_str(), "wb");
    if(file == nullptr){
        fclose(file);
        std::cout << "Creation fail" << std::endl;
        return -1;
    }

    fclose(file);
    return 0;
}

RC PagedFileManager::destroyFile(const std::string &fileName) {
    FILE* file = fopen(fileName.c_str(), "rb");
    if(file == nullptr){
        std::cout << "File not exists" << std::endl;
        return -1;
    }
    fclose(file);
    int r = remove(fileName.c_str());
    if(r != 0){
        std::cout << "Deletion fails" << std::endl;
        return -1;
    }

    return 0;
}

RC PagedFileManager::openFile(const std::string &fileName, FileHandle &fileHandle) {
    if (fileHandle.isOccupied()) return 1;
    FILE* file = fopen(fileName.c_str(), "rb");
    if(file == nullptr){
        fclose(file);
        std::cout << "File not exists" << std::endl;
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
    readPageCounter = (Counter) *((char *) data + sizeof(char));
    writePageCounter = (Counter) *((char *) data + sizeof(char) + sizeof(Counter));
    appendPageCounter = (Counter) *((char *) data + sizeof(char) + sizeof(Counter) * 2);
    totalPageNum = (PageNum) *((char *) data + sizeof(char) + sizeof(Counter) * 3);
    dataPageNum = (PageNum) *((char *) data + sizeof(char) + sizeof(Counter) * 3 + sizeof(PageNum));
    free(data);
    return 0;
}

RC FileHandle::writeHiddenPage() {
    if (handle == nullptr) return 1;
    //void *data = malloc(sizeof(unsigned) * 3 + sizeof(char) + sizeof(PageNum));
    void *data = malloc(PAGE_SIZE);
    if (data == nullptr) throw std::bad_alloc();
    *((char *) data) = 'Y';
    *(Counter *)((char *) data + sizeof(char)) = readPageCounter;
    *(Counter *)((char *) data + sizeof(char) + sizeof(Counter)) = writePageCounter;
    *(Counter *)((char *) data + sizeof(char) + sizeof(Counter) * 2) = appendPageCounter;
    *(PageNum *)((char *) data + sizeof(char) + sizeof(Counter) * 3) = totalPageNum;
    *(PageNum *)((char *) data + sizeof(char) + sizeof(Counter) * 3 + sizeof(PageNum)) = dataPageNum;
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

RC FileHandle::appendPage(const void *data, bool dataPage) {
    if (dataPage) dataPageNum++;
    handle->seekp(totalPageNum * PAGE_SIZE, std::ios::beg);
    handle->write((const char *) data, PAGE_SIZE);
    appendPageCounter++;
    totalPageNum++;
    if (dataPage && dataPageNum % PAGE_SIZE == 0) {
        // need to insert a new FSP TODO add tests
        void *fsp = malloc(PAGE_SIZE);
        appendPage(fsp, false);
        free(fsp);
    }
    return 0;
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


// ========================================================================================
//                                 Free Space Page Class
// ========================================================================================

FreeSpacePage::FreeSpacePage(void *data) {
    assert(data != nullptr);
    page = data;
}

FreePageSpace FreeSpacePage::getFreeSpace(PageNum pageIndex) {
    assert(pageIndex < PAGE_SIZE);
    return *((FreePageSpace *) ((char *) page + pageIndex));
}

void FreeSpacePage::writeFreeSpace(PageNum pageIndex, FreePageSpace freePageSpace) {
    assert(pageIndex < PAGE_SIZE);
    *((FreePageSpace *) ((char *) page + pageIndex)) = freePageSpace;
}