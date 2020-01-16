#include <iostream>
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
        fclose(file);
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
    FILE* file = fopen(fileName.c_str(), "rb");
    if(file == nullptr){
        fclose(file);
        std::cout << "File not exists" << std::endl;
        return -1;
    }
    fclose(file);
    std::fstream* f = new std::fstream;
    f->open(fileName, std::ios::in | std::ios::out | std::ios::binary);
    return fileHandle.setHandle(f);
}

RC PagedFileManager::closeFile(FileHandle &fileHandle) {
    fileHandle.releaseHandle();
    return 0;
}

FileHandle::FileHandle() {
    readPageCounter = 0;
    writePageCounter = 0;
    appendPageCounter = 0;
    totalPage = 0;
    handle = nullptr;
}

FileHandle::~FileHandle() {
    free(handle);
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
    RC rc = readPage(-1, data);
    if (rc != 0) return 1;
    if (*((char *) data) != 'Y') return 1;
    readPageCounter = (unsigned) *((char *) data + sizeof(char));
    writePageCounter = (unsigned) *((char *) data + sizeof(char) + sizeof(unsigned));
    appendPageCounter = (unsigned) *((char *) data + sizeof(char) + sizeof(unsigned) * 2);
    totalPage = (PageNum) *((char *) data + sizeof(char) + sizeof(unsigned) * 3);
    free(data);
    return 0;
}

RC FileHandle::writeHiddenPage() {
    if (handle == nullptr) return 1;
    //void *data = malloc(sizeof(unsigned) * 3 + sizeof(char) + sizeof(PageNum));
    void *data = malloc(PAGE_SIZE);
    *((char *) data) = 'Y';
    *(unsigned *)((char *) data + sizeof(char)) = readPageCounter;
    *(unsigned *)((char *) data + sizeof(char) + sizeof(unsigned)) = writePageCounter;
    *(unsigned *)((char *) data + sizeof(char) + sizeof(unsigned) * 2) = appendPageCounter;
    *(PageNum *)((char *) data + sizeof(char) + sizeof(unsigned) * 3) = totalPage;
    writePage(-1, data);
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

RC FileHandle::readPage(int pageNum, void *data) {
    if (pageNum > 0 && pageNum >= totalPage) return -1;  // note: prevent negative int converting to unsigned
    pageNum += 1;  // because the hidden page is the first page
    handle->clear();
    handle->seekg(pageNum * PAGE_SIZE, std::ios::beg);
    handle->read((char*) data, PAGE_SIZE);
    readPageCounter++;
    return 0;
}

RC FileHandle::writePage(int pageNum, const void *data) {
    if (pageNum > 0 && pageNum >= totalPage) return -1;  // note: prevent negative int converting to unsigned
    pageNum += 1;
    handle->clear();
    handle->seekp(pageNum * PAGE_SIZE, std::ios::beg);
    handle->write((const char*) data, PAGE_SIZE);
    writePageCounter++;
    return 0;
}

RC FileHandle::appendPage(const void *data) {
    handle->seekp((totalPage + 1) * PAGE_SIZE, std::ios::beg);
    handle->write((const char *) data, PAGE_SIZE);
    appendPageCounter++;
    totalPage += 1;
    return 0;
}

unsigned FileHandle::getNumberOfPages() {
    return totalPage;
}

RC FileHandle::collectCounterValues(unsigned &readPageCount, unsigned &writePageCount, unsigned &appendPageCount) {
    readPageCount = this->readPageCounter;
    writePageCount = this->writePageCounter;
    appendPageCount = this->appendPageCounter;
    return 0;
}