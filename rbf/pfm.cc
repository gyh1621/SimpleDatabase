#include <fstream>
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
    if(file != NULL){
        fclose(file);
        std::cout << "filename already exists" << std::endl;
        return -1;
    }

    file = fopen(fileName.c_str(), "wb");
    if(file == NULL){
        fclose(file);
        std::cout << "Creation fail" << std::endl;
        return -1;
    }

    fclose(file);
    return 0;
}

RC PagedFileManager::destroyFile(const std::string &fileName) {
    FILE* file = fopen(fileName.c_str(), "rb");
    if(file == NULL){
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
    if(file == NULL){
        fclose(file);
        std::cout << "File not exists" << std::endl;
        return -1;
    }
    fclose(file);
    std::fstream f;
    f.open(fileName, std::ios::in | std::ios::out | std::ios::binary);
    return fileHandle.setHandle(f);
}

RC PagedFileManager::closeFile(FileHandle &fileHandle) {
    return fileHandle.releaseHandle();
}

FileHandle::FileHandle() {
    readPageCounter = 0;
    writePageCounter = 0;
    appendPageCounter = 0;
    totalPage = 0;
    handle = nullptr;
}

FileHandle::~FileHandle() = default;

RC FileHandle::readHiddenPage(){
    void *data = malloc(sizeof(unsigned) * 4 + sizeof(char));
    readPage(-1, data);
    if (*((char *) data) != 'Y') return 1;
    readPageCounter = (int) *((char *) data + sizeof(char));
    writePageCounter = (int) *((char *) data + sizeof(unsigned));
    appendPageCounter = (int) *((char *) data + sizeof(unsigned) * 2);
    totalPage = (int) *((char *) data + sizeof(unsigned) * 3);
    free(data);
    return 0;
}

RC FileHandle::writeHiddenPage() {
    if (handle == nullptr) return 1;
    void *data = malloc(sizeof(unsigned) * 3 + sizeof(char));
    *((char *) data) = 'Y';
    *((char *) data + sizeof(char)) = readPageCounter;
    *((char *) data + sizeof(char) + sizeof(unsigned)) = writePageCounter;
    *((char *) data + sizeof(char) + sizeof(unsigned) * 2) = appendPageCounter;
    *((char *) data + sizeof(char) + sizeof(unsigned) * 3) = totalPage;
    writePage(-1, data);
    free(data);
    return 0;
}

RC FileHandle::setHandle(std::fstream &f) {
    if (handle != nullptr) {
        // a file is already opened
        return 1;
    }
    handle = &f;
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
        writeHiddenPage();
        handle->close();
        return 0;
    }
}

RC FileHandle::readPage(PageNum pageNum, void *data) {
    // TODO
    // add test: detect page num is valid
    pageNum += 1;  // because the hidden page is the first page
    handle->seekp(pageNum * PAGE_SIZE, std::ios::beg);
    handle->read((char*) data, PAGE_SIZE);
    readPageCounter++;
    return 0;
}

RC FileHandle::writePage(PageNum pageNum, const void *data) {
   pageNum += 1;
   handle->seekp(pageNum * PAGE_SIZE, std::ios::beg);
   handle->write((const char*) data, PAGE_SIZE);
   writePageCounter++;
   return 0;
}

RC FileHandle::appendPage(const void *data) {
    totalPage += 1;
    handle->seekp(0, std::ios::end);
    handle->write((const char *) data, PAGE_SIZE);
    appendPageCounter++;
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