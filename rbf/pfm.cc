#include <iostream>
#include <cmath>
#include <cassert>
#include <cstring>
#include "pfm.h"
#include "record.h"


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
    RC r = remove(fileName.c_str());
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
    curFSPNum = 0;
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
        if (fspData != nullptr) {
            // save free space to disk
            writePage(curFSPNum, fspData, true);
            free(fspData);
            fspData = nullptr;
        }
        curFSPNum = 0;
        // write counters and total page number
        writePageCounter += 1;  // need to count the last write
        writeHiddenPage();
        handle->close();
        delete(handle);
        handle = nullptr;
        return 0;
    }
}

bool FileHandle::isOccupied() {
    return handle != nullptr;
}

PageNum FileHandle::changeToActualPageNum(PageNum dataPageNum) {
    dataPageNum += 1;  // hidden page
    return ceil(float(dataPageNum) / PAGES_IN_FSP) + dataPageNum;
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

RC FileHandle::writePage(PageNum pageNum, PageFreeSpace freeSpace, const void *data) {
    // update page's free space
    updateFreeSpaceOfPage(pageNum, freeSpace);
    return writePage(pageNum, data);
}

RC FileHandle::appendPage(const void *data, bool dataPage) {
    if (dataPage) dataPageNum++;
    handle->seekp(totalPageNum * PAGE_SIZE, std::ios::beg);
    handle->write((const char *) data, PAGE_SIZE);
    appendPageCounter++;
    totalPageNum++;
    if (dataPage && dataPageNum % (PAGES_IN_FSP) == 0) {
        // need to insert a new FSP
        void *fsp = malloc(PAGE_SIZE);
        memset(fsp, 0, PAGE_SIZE);
        appendPage(fsp, false);
        free(fsp);
    }
    return 0;
}

RC FileHandle::appendPage(PageFreeSpace freeSpace, const void *data) {
    RC rc = appendPage(data);
    // update page's free space
    updateFreeSpaceOfPage(dataPageNum - 1, freeSpace);
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

void FileHandle::getFSPofPage(const PageNum &dataPageNum, PageNum &fspNum, PageNum &pageIndex) {
    fspNum = (dataPageNum / PAGES_IN_FSP) * (PAGES_IN_FSP + 1) + 1;
    pageIndex = dataPageNum % PAGES_IN_FSP;
}

RC FileHandle::updateCurFSP(const PageNum &fspNum) {
    // load new fsp
    if (fspNum != curFSPNum) {
        if (fspData == nullptr) {
            // std::cout << "init fsp data " << std::endl; // debug
            fspData = malloc(PAGE_SIZE);
            if (fspData == nullptr) throw std::bad_alloc();
        } else {
            // need to save free space to disk first
            writePage(curFSPNum, fspData, true);
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

PageFreeSpace FileHandle::getFreeSpaceOfPage(PageNum dataPageNum) {
    // get fsp page number and pageIndex in that fsp and update current fsp member
    assert(dataPageNum < this->dataPageNum);
    PageNum fspNum, pageIndex;
    getFSPofPage(dataPageNum, fspNum, pageIndex);
    updateCurFSP(fspNum);
    return curFSP.getFreeSpace(pageIndex);
}

void FileHandle::updateFreeSpaceOfPage(PageNum dataPageNum, PageFreeSpace freePageSpace) {
    PageNum fspNum, pageIndex;
    getFSPofPage(dataPageNum, fspNum, pageIndex);
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

PageFreeSpace FreeSpacePage::getFreeSpace(PageNum pageIndex) {
    assert(pageIndex < PAGES_IN_FSP);
    PageFreeSpace freePageSpace;
    memcpy(&freePageSpace, (char *) page + pageIndex * sizeof(PageFreeSpace), sizeof(PageFreeSpace));
    return freePageSpace;
}

void FreeSpacePage::writeFreeSpace(PageNum pageIndex, PageFreeSpace freePageSpace) {
    assert(pageIndex < PAGES_IN_FSP);
    memcpy((char *) page + pageIndex * sizeof(PageFreeSpace), &freePageSpace, sizeof(PageFreeSpace));
}


// ========================================================================================
//                                  Data Page Class
// ========================================================================================

DataPage::DataPage(void *data) {
    page = data;

    // Init page
    auto recordNumberOffset = PAGE_SIZE - InfoSize;
    auto slotNumberOffset = recordNumberOffset + sizeof(RecordNumber);
    auto initIndicatorOffset = PAGE_SIZE - sizeof(InitIndicator);
    InitIndicator isInited;
    memcpy(&isInited, (char *) page + initIndicatorOffset, sizeof(InitIndicator));
    if (isInited == 'Y') {
        // read last info section
        memcpy(&recordNumber, (char *) page + recordNumberOffset, sizeof(RecordNumber));
        memcpy(&slotNumber, (char *) page + slotNumberOffset, sizeof(SlotNumber));
    } else {
        // init page info
        recordNumber = 0;
        slotNumber = 0;
        isInited = 'Y';
        memcpy((char *) page + initIndicatorOffset, &isInited, sizeof(InitIndicator));
    }
    // compute free space start offset and free space
    auto slot = slotNumber - 1;
    freeSpaceOffset = 0;
    SlotPointerIndicator isPointer;
    RecordOffset recordOffset;
    RecordLength recordLength;
    while (slot >= 0) {
        parseSlot(slot, isPointer, recordOffset, recordLength);
        if (!isPointer && recordOffset != DeletedRecordOffset) {
            freeSpaceOffset = recordOffset + recordLength;
            break;
        }
        slot--;
    }
    freeSpace = getNthSlotOffset(slotNumber - 1) - freeSpaceOffset;
}

int DataPage::getNthSlotOffset(SlotNumber n) {
//    assert(n < slotNumber);
    return PAGE_SIZE - (InfoSize + SlotSize * (n + 1));
}

SlotNumber DataPage::getFirstAvailableSlot() {
    // try to find deleted slots
    for (SlotNumber slot = 0; slot < slotNumber; slot++) {
        auto recordOffsetPos = getNthSlotOffset(slot) + sizeof(SlotPointerIndicator);
        RecordOffset recordOffset;
        memcpy(&recordOffset, (char *) page + recordOffsetPos, sizeof(RecordOffset));
        if (recordOffset == DeletedRecordOffset) return slot;
    }
    // no unused slots, create a new slot
    return slotNumber;
}

void DataPage::parseSlot(int slot, SlotPointerIndicator &isPointer, RecordOffset &recordOffset, RecordLength &recordLen){
    auto slotOffset = getNthSlotOffset(slot);
    isPointer = *(SlotPointerIndicator *)((char *) page + slotOffset);
    slotOffset += sizeof(SlotPointerIndicator);
    memcpy(&recordOffset, (char *) page + slotOffset, sizeof(RecordOffset));
    slotOffset += sizeof(RecordOffset);
    memcpy(&recordLen, (char *) page + slotOffset, sizeof(RecordLength));
}

void DataPage::deleteSlot(int slot) {
    auto slotOffset = getNthSlotOffset(slot);
    *((SlotPointerIndicator *)((char *)page + slotOffset)) = false;
    slotOffset += sizeof(SlotPointerIndicator);
    memcpy((char *) page + slotOffset, &DeletedRecordOffset, sizeof(RecordOffset));
}

void DataPage::updateSlotInfo(RecordOffset offset, RecordLength length, bool dir) {
    int slotOffset = 0;
    for(int i = 0; i < slotNumber; i++){
        slotOffset = getNthSlotOffset(i);
        bool isPointer;
        memcpy(&isPointer, (char *)page + slotOffset, sizeof(bool));
        if(isPointer){
            continue;
        }
        slotOffset += sizeof(SlotPointerIndicator);
        RecordOffset curOffset;
        memcpy(&curOffset, (char *)page + slotOffset, sizeof(RecordOffset));
        if(curOffset == DeletedRecordOffset){
            continue;
        }
        if(curOffset > offset){
            if(dir){
                curOffset -= length;
            }else{
                curOffset += length;
            }
            memcpy((char *) page + slotOffset, &curOffset, sizeof(RecordOffset));
        }
    }

}

void DataPage::moveRecords(RecordOffset startOffset, RecordOffset targetOffset) {
    auto moveSize = this->freeSpaceOffset - startOffset;
    //std::cout << targetOffset << " " << startOffset << " " << moveSize << std::endl;  //debug
    memmove((char *) this->page + targetOffset, (char *) this->page + startOffset, moveSize);
    this->freeSpaceOffset = targetOffset + moveSize;
}

void DataPage::printSlots() {
    SlotNumber slotPerLine = 10;
    RecordLength recordLength;
    RecordOffset recordOffset;
    SlotPointerIndicator isPointer;
    for (SlotNumber i = 0; i < slotNumber; i++) {
        if (i != 0 && i % slotPerLine == 0) std::cout << std::endl;
        parseSlot(i, isPointer, recordOffset, recordLength);
        if (isPointer) std::cout << "slot " << i << ": pointer to " << recordOffset << " " << recordLength << "\t";
        else if (recordOffset == DeletedRecordOffset) std::cout << "slot " << i << ": deleted\t";
        else std::cout << "slot" << i << ": " << isPointer << " " << recordOffset << " " << recordLength << "\t";
    }
    std::cout << std::endl;
}

SlotNumber DataPage::insertRecord(Record &record) {
    // insert record
    auto startOffset = freeSpaceOffset;
    memcpy((char *) page + freeSpaceOffset, record.getRecordData(), record.getSize());
    //std::cout << "Insert one record " << "at: " << startOffset  // debug
    //          << " end:" << startOffset + record.getSize() << "\t";
    recordNumber++;
    // get first available slot
    SlotNumber slot = getFirstAvailableSlot();
    // update free space
    freeSpaceOffset += record.getSize();
    freeSpace -= record.getSize();
    if (slot == slotNumber) freeSpace -= SlotSize;
    // insert the corresponding slot
    auto offset = getNthSlotOffset(slot);
    //std::cout << " SLOT at: " << offset << " end: " << offset + SlotSize << "\t";  // debug
    *((SlotPointerIndicator *) ((char *) page + offset)) = false;
    offset += sizeof(SlotPointerIndicator);
    memcpy((char *) page + offset, &startOffset, sizeof(RecordOffset));
    offset += sizeof(RecordOffset);
    auto recordSize = record.getSize();
    memcpy((char *) page + offset, &recordSize, sizeof(RecordLength));
    // update slot number
    if (slot == slotNumber) slotNumber++;
    return slot;  // slot index starts from 0
}

RecordLength DataPage::getRecordSize(SlotNumber slot) {
    RecordOffset offset;
    RecordLength length;
    SlotPointerIndicator isPointer;

    parseSlot(slot, isPointer, offset, length);
    assert(isPointer == 0); // should not be a pointer

    return length;
}

void *DataPage::readRecord(SlotNumber slot) {
    RecordOffset offset;
    RecordLength length;
    SlotPointerIndicator isPointer;

    parseSlot(slot, isPointer, offset, length);
    if (isPointer || offset == DeletedRecordOffset) return nullptr;

    void *data = malloc(length);
    if (data == nullptr) throw std::bad_alloc();
    memcpy(data, (char *) page + offset, length);

    return data;
}

void DataPage::readRecordIntoRaw(const SlotNumber slot, const std::vector<Attribute> &recordDescriptor, void* data) {
    void *record = readRecord(slot);
    assert(record != nullptr);

    Record r(record);
    r.convertToRawData(recordDescriptor, data);
    free(record);
    //std::cout << "Read one record at " << offset << " end " << offset + length << "\t"; // debug
}

//todo: check slot number;
void DataPage::deleteRecord(SlotNumber &slotid) {
    RecordOffset offset;
    RecordLength length;
    SlotPointerIndicator isPointer;
    parseSlot(slotid, isPointer, offset, length);
    assert(isPointer == 0);  // should not be a pointer
    updateSlotInfo(offset, length, true);
    moveRecords(offset + length, offset);
    recordNumber--;
    freeSpace += length;
    freeSpaceOffset -= length;
    deleteSlot(slotid);
}

void DataPage::moveRecord(SlotNumber slot, const RID &newRID) {
    deleteRecord(slot);
    auto slotOffset = getNthSlotOffset(slot);
    SlotPointerIndicator isPointer = true;
    memcpy((char *) page + slotOffset, &isPointer, sizeof(SlotPointerIndicator));
    slotOffset += sizeof(SlotPointerIndicator);
    memcpy((char *) page + slotOffset, &newRID.pageNum, sizeof(RecordOffset));
    slotOffset += sizeof(RecordOffset);
    memcpy((char *) page + slotOffset, &newRID.slotNum, sizeof(RecordLength));
}

void DataPage::updateRecord(Record &updateRecord, RecordLength &slotid) {
    RecordOffset offset;
    RecordLength length;
    SlotPointerIndicator isPointer;
    parseSlot(slotid, isPointer, offset, length);
    assert(isPointer == 0);
    RecordLength newLength = updateRecord.getSize();
    if(length < updateRecord.getSize()){
        int movedLen = updateRecord.getSize() - length;
        moveRecords(offset + length, offset + length + movedLen);
        memcpy((char*)page + offset, updateRecord.getRecordData(), newLength);
        // update length in the slot
        auto slotOffset = getNthSlotOffset(slotid) + sizeof(SlotPointerIndicator) + sizeof(RecordOffset);
        memcpy((char *) page + slotOffset, &newLength, sizeof(RecordLength));
        updateSlotInfo(offset, movedLen, false);
        freeSpace -= movedLen;
        freeSpaceOffset += movedLen;
    }else{
        int movedLen = length - newLength;
        moveRecords(offset + length, offset + length - movedLen);
        memcpy((char*)page + offset, updateRecord.getRecordData(), newLength);
        // update length in the slot
        auto slotOffset = getNthSlotOffset(slotid) + sizeof(SlotPointerIndicator) + sizeof(RecordOffset);
        memcpy((char *) page + slotOffset, &newLength, sizeof(RecordLength));
        updateSlotInfo(offset, movedLen, true);
        freeSpace += movedLen;
        freeSpaceOffset -= movedLen;
    }
}

int DataPage::checkRecordExist(SlotNumber &slotid, RID &newRID) {
    RecordOffset curOffset;
    RecordLength curLength;
    SlotPointerIndicator isPointer;
    parseSlot(slotid, isPointer, curOffset, curLength);
    if(isPointer){
        newRID.pageNum = curOffset;
        newRID.slotNum = curLength;
        return -1;
    } else if (curOffset == DeletedRecordOffset) {
        return 1;  // deleted record
    } else {
        return 0;
    }
}

const void * DataPage::getPageData() {
    auto offset = PAGE_SIZE - InfoSize;
    memcpy((char *) page + offset, &recordNumber, sizeof(RecordNumber));
    offset += sizeof(RecordNumber);
    memcpy((char *) page + offset, &slotNumber, sizeof(SlotNumber));
    return page;
}

const PageFreeSpace DataPage::getFreeSpace() { return freeSpace; }
const RecordNumber DataPage::getRecordNumber() { return recordNumber; }

SlotNumber DataPage::getSlotNumber() { return slotNumber; }


// ========================================================================================
//                                     Record Class
// ========================================================================================

int Record::getNullIndicatorSize(const int &fieldNumber) {
    return ceil(fieldNumber / 8.0);
}

bool Record::isFieldNull(const int &fieldIndex, const void *nullIndicatorData) {
    // get the byte field-bit inside
    int byteIndex = fieldIndex / 8;
    unsigned nullIndicator;
    memcpy(&nullIndicator, (char *) nullIndicatorData + byteIndex, sizeof(unsigned));
    // get field bit's index in the byte
    unsigned fieldBitIndex = 8 - fieldIndex % 8 - 1;
    // get field bit
    unsigned fieldBit = (nullIndicator & ( 1 << fieldBitIndex)) >> fieldBitIndex;
    return fieldBit == 1;
}

int Record::getRecordActualSize(const int &nullIndicatorSize, const std::vector<Attribute> &recordDescriptor, const void *data) {
    // get initial size: header size + field offset section size
    auto size = Record::RecordHeaderSize + recordDescriptor.size() * sizeof(FieldOffset);
    auto offset = nullIndicatorSize;

    // add fields' data size
    auto *charLength = new unsigned;
    for (auto it = recordDescriptor.begin(); it != recordDescriptor.end(); it++) {
        int fieldIndex = std::distance(recordDescriptor.begin(), it);
        if (isFieldNull(fieldIndex, data)) continue;
        switch (it->type) {
            case AttrType::TypeInt: size += sizeof(int); offset += sizeof(int); break;
            case AttrType::TypeReal: size += sizeof(float); offset += sizeof(int); break;
            case AttrType::TypeVarChar:
                memcpy(charLength, (char *) data + offset, sizeof(unsigned));
                //unsigned charLength = *((unsigned *) ((char *) data + offset));  // 4 bytes in raw data
                size += *charLength;
                offset += sizeof(unsigned) + *charLength;
                break;
            default: break;
        }
    }
    delete(charLength);

    return size;
}

Record::Record(const std::vector<Attribute> &recordDescriptor, const void *data, const RecordVersion version) {
    passedData = false;
    int fieldNum = recordDescriptor.size();
    int nullIndicatorSize = getNullIndicatorSize(fieldNum);
    this->size = getRecordActualSize(nullIndicatorSize, recordDescriptor, data);
    this->record = malloc(this->size);
    if (this->record == nullptr) throw std::bad_alloc();
    this->recordVersion = version;
    this->fieldNumber = fieldNum;
    // offset1 is the current writing position of *record
    // offset2 is the current reading position of *data
    auto offset1 = 0, offset2 = 0;

    // write version
    memcpy(record, &this->recordVersion, sizeof(RecordVersion));
    offset1 += sizeof(RecordVersion);

    // write field number
    memcpy((char *) record + offset1, &this->fieldNumber, sizeof(FieldNumber));
    offset1 += sizeof(FieldNumber);

    // record: jump over offset section
    offset1 += recordDescriptor.size() * sizeof(FieldOffset);

    // data: jump over null indicator data
    offset2 += nullIndicatorSize;

    // copying fields
    bool nullBit;
    auto *varcharLength = new unsigned;
    for (auto it = recordDescriptor.begin(); it != recordDescriptor.end(); it++) {
        int index = std::distance(recordDescriptor.begin(), it);  // field index
        nullBit = isFieldNull(index, data);  // get null bit
        auto currentFieldOffsetPos = RecordHeaderSize + index * sizeof(FieldOffset);
        if (nullBit) {
            // null field
            *((FieldOffset *)((char *) record + currentFieldOffsetPos)) = 0;
        } else {
            // copy field
            AttrType type = it->type;
            if (type == AttrType::TypeInt) {
                memcpy((char *) record + offset1, (char *) data + offset2, it->length);
                offset1 += it->length;
                offset2 += it->length;
            } else if (type == AttrType::TypeReal) {
                memcpy((char *) record + offset1, (char *) data + offset2, it->length);
                offset1 += it->length;
                offset2 += it->length;
            } else if (type == AttrType::TypeVarChar) {
                // read length byte
                memcpy(varcharLength, (char *) data + offset2, sizeof(unsigned));
                offset2 += sizeof(unsigned);
                // copy varchar
                memcpy((char *) record + offset1, (char *) data + offset2, *varcharLength);
                offset1 += *varcharLength;
                offset2 += *varcharLength;
            }
            *((FieldOffset *)((char *) record + currentFieldOffsetPos)) = (FieldOffset) offset1;
        }
    }
    delete(varcharLength);

    assert(this->size == offset1);
}

Record::Record(void* data){
    passedData = true;
    this->record = data;
    memcpy(&this->recordVersion, data, sizeof(RecordVersion));
    memcpy(&this->fieldNumber, (char *) data + sizeof(RecordVersion), sizeof(FieldNumber));
    if (this->fieldNumber == 0) {
        this->size = RecordHeaderSize;
    } else {
        FieldOffset lastFieldOffsetPos = RecordHeaderSize + (this->fieldNumber - 1) * sizeof(FieldOffset);
        FieldOffset lastFieldOffset;
        memcpy(&lastFieldOffset, (char *) this->record + lastFieldOffsetPos, sizeof(FieldOffset));
        this->size = lastFieldOffset;
    }
}

int Record::getSize() { return size; }

const void * Record::getRecordData() {
    return record;
}

void Record::convertToRawData(const std::vector<Attribute> &recordDescriptor, void *data) {
    FieldNumber actualFieldNumber = 0;
    for (Attribute attr: recordDescriptor) {
        if (attr.type == AttrType::TypeNull) continue;
        else actualFieldNumber++;
    }
    // init null indicators
    auto nullPointerSize = getNullIndicatorSize(actualFieldNumber);
    auto* nullPointer = (unsigned char*)data;
    for (int i = 0; i < nullPointerSize; i++) nullPointer[i] = 0;

    // jump over record header
    auto startOffset = RecordHeaderSize;

    // field offsets
    auto lastFieldEndOffset = startOffset + fieldNumber * sizeof(FieldOffset);
    auto fieldEndOffset = 0;
    // pointer of field value section of data
    int dataOffset = nullPointerSize;

    int jumped = 0;
    for(int i = 0; i < fieldNumber; i++){
        fieldEndOffset = *((FieldOffset *)((char *)record + startOffset + sizeof(FieldOffset) * i));

        Attribute attr = recordDescriptor[i];
        if (attr.type == AttrType::TypeNull) {
            lastFieldEndOffset = fieldEndOffset == 0 ? lastFieldEndOffset : fieldEndOffset;
            jumped++;
            continue;
        }

        int byteNum = (i - jumped) / 8;
        auto fieldLength = fieldEndOffset - lastFieldEndOffset;
        if(fieldEndOffset == 0){  // null field
            nullPointer[byteNum] |= ((char) 1) << (8 - (i % 8) - 1);
        } else {  // data field
            if (attr.type == AttrType::TypeVarChar) {
                memcpy((char*)data + dataOffset, &fieldLength, sizeof(unsigned));
                dataOffset += 4;
            }
            memcpy((char*)data + dataOffset, (char *)record + lastFieldEndOffset, fieldLength);
            dataOffset += fieldLength;
            lastFieldEndOffset = fieldEndOffset;
        }
    }
}

void Record::printRecord(const std::vector<Attribute> &recordDescriptor) {
    // jump over record header
    auto startOffset = RecordHeaderSize;

    // field offsets
    auto lastFieldEndOffset = startOffset + fieldNumber * sizeof(FieldOffset);
    auto fieldEndOffset = 0;

    int intAttrVal;
    float realAttrVal;
    for (int i = 0; i < fieldNumber; i++) {
        Attribute attr = recordDescriptor[i];
        std::cout << attr.name << ": ";
        fieldEndOffset = *((FieldOffset *)((char *)record + startOffset + sizeof(FieldOffset) * i));
        auto fieldLength = fieldEndOffset - lastFieldEndOffset;
        if (fieldEndOffset == 0){
            std::cout << "NULL";
        } else {
            switch(attr.type){
                default: break;
                case AttrType::TypeInt:
                    memcpy(&intAttrVal, (char *)record + lastFieldEndOffset, sizeof(int));
                    std::cout << intAttrVal;
                    break;
                case AttrType::TypeReal:
                    memcpy(&realAttrVal, (char *)record + lastFieldEndOffset, sizeof(float));
                    std::cout << realAttrVal;
                    break;
                case AttrType::TypeVarChar:
                    char* varchar = new char[fieldLength];
                    memcpy(varchar, (char *) record + lastFieldEndOffset, fieldLength);
                    std::string s(varchar, fieldLength);
                    std::cout << s;
                    delete[](varchar);
                    break;
            }
            lastFieldEndOffset = fieldEndOffset;
        }
        if (i == fieldNumber - 1) {
            std::cout << std::endl;
        } else {
            std::cout << " ";
        }
    }
}

void *Record::getFieldValue(const FieldNumber &fieldIndex, AttrLength &fieldLength) {
    FieldOffset startOffset, endOffset;
    if (fieldIndex == 0) {
        startOffset = RecordHeaderSize + fieldNumber * sizeof(FieldOffset);
    } else {
        memcpy(&startOffset, (char *) record + RecordHeaderSize + (fieldIndex - 1) * sizeof(FieldOffset), sizeof(FieldOffset));
    }
    memcpy(&endOffset, (char *) record + RecordHeaderSize + fieldIndex * sizeof(FieldOffset), sizeof(FieldOffset));
    fieldLength = endOffset - startOffset;
    void *data = malloc(fieldLength);
    if (data == nullptr) throw std::bad_alloc();
    memcpy(data, (char *) record + startOffset, fieldLength);
    return data;
}

std::string Record::getString(const void *data, AttrLength attrLength) {
    std::string str;
    for (FieldOffset i = 0; i < attrLength; i++) {
        str += *((char *) data + i);
    }
    return str;
}


Record::~Record() {
    if (!passedData) free(this->record);
};
