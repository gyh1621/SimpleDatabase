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
    readHiddenPage();
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


// =====================================================================================
//                                    Node Page
// =====================================================================================

NodePage::NodePage(void *pageData, bool init) {
    if (!init) {
        // read information from pageData
        this->pageData = pageData;
        PageOffset offset = PAGE_SIZE - sizeof(bool);
        memcpy(&isLeaf, (char *) pageData + offset, sizeof(bool));
        offset -= sizeof(SlotNumber);
        memcpy(&slotNumber, (char *) pageData + offset, sizeof(SlotNumber));
        offset -= sizeof(PageFreeSpace);
        memcpy(&freeSpace, (char *) pageData + offset, sizeof(PageFreeSpace));
    } else {
        // init
        this->pageData = pageData;
        slotNumber = 0;
        // note: need update freeSpace in LeafNodePage's constructor
        freeSpace = PAGE_SIZE - infoSectionLength;
        // write to pageData
        PageOffset offset = PAGE_SIZE - sizeof(bool);
        memcpy((char *) pageData + offset, &isLeaf, sizeof(bool));
        offset -= sizeof(SlotNumber);
        memcpy((char *) pageData + offset, &slotNumber, sizeof(SlotNumber));
        offset -= sizeof(PageFreeSpace);
        memcpy((char *) pageData + offset, &freeSpace, sizeof(PageFreeSpace));
    }
}

PageOffset NodePage::getFreeSpaceOffset() {
    return PAGE_SIZE - infoSectionLength - sizeof(PageOffset) * slotNumber - freeSpace;
}

void NodePage::moveData(const PageOffset &startOffset, const PageOffset &targetOffset, const PageOffset &length) {
    memmove((char *) this->pageData + targetOffset, (char *) this->pageData+ startOffset, length);
}

void NodePage::updateSlots(const SlotNumber &startSlot, const SlotNumber &endSlot, const PageOffset &deviateOffset,
                           bool add) {
    if (startSlot >= slotNumber || endSlot >= slotNumber) {
        throw std::invalid_argument("slot exceeds range");
    }
    for (SlotNumber slot = startSlot; slot <= endSlot; slot++) {
        PageOffset oldOffset = getNthKeyOffset(slot);
        PageOffset newOffset;
        if (add) newOffset = oldOffset + deviateOffset;
        else newOffset = oldOffset - deviateOffset;
        writeNthSlot(slot, newOffset);
    }
}

PageOffset NodePage::getNthSlotOffset(const KeyNumber &keyIndex) {
    if (keyIndex >= slotNumber) {
        throw std::invalid_argument("key index exceeds range.");
    }
    auto offset = PAGE_SIZE - infoSectionLength;
    offset -= (keyIndex + 1) * sizeof(PageOffset);
    return offset;
}

PageOffset NodePage::getNthKeyOffset(const KeyNumber &keyIndex) {
    if (keyIndex >= slotNumber) {
        throw std::invalid_argument("key index exceeds range.");
    }
    auto slotOffset = getNthSlotOffset(keyIndex);
    PageOffset keyOffset;
    memcpy(&keyOffset, (char *) pageData + slotOffset, sizeof(PageOffset));
    return keyOffset;
}

void NodePage::writeNthSlot(const KeyNumber &keyIndex, const PageOffset &keyOffset) {
    if (keyIndex >= slotNumber) {
        throw std::invalid_argument("key index exceeds range.");
    }
    auto slotOffset = getNthSlotOffset(keyIndex);
    memcpy((char *) pageData + slotOffset, &keyOffset, sizeof(PageOffset));
}

bool NodePage::isLeafNode(void *data) {
    int leaf;
    memcpy(&leaf, (char *) data + PAGE_SIZE - sizeof(bool), sizeof(bool));
    return leaf == int(true);
}

PageOffset NodePage::getKeyLength(const void *key, const AttrType &attrType) {
    PageOffset keyLength;
    switch(attrType) {
        case TypeInt: keyLength = sizeof(int); break;
        case TypeReal: keyLength = sizeof(float); break;
        case TypeVarChar: memcpy(&keyLength, key, 4); keyLength += 4; break;
        default: throw std::invalid_argument("invalid attr type");
    }
    return keyLength;
}

bool NodePage::findKey(const void *key, const AttrType &attrType, SlotNumber &keyIndex) {
    if (slotNumber == 0) {
        keyIndex = 0;
        return false;
    }
    long l = 0, r = slotNumber - 1, mid;
    while (l <= r) {
        mid = (l + r) / 2;
        void *slotData = getNthKey(mid, attrType);
        auto res = compare(slotData, key, attrType);
        free(slotData);
        if (res > 0) {
            r = mid - 1;
        } else if (res < 0) {
            l = mid + 1;
        } else {
            keyIndex = mid;
            return true;
        }
    }
    keyIndex = l;
    return false;
}

RC NodePage::compare(const void *data1, const void *data2, const AttrType &attrType) {
    if (attrType == TypeReal) {
        float f1 = *((float *) data1), f2 = *((float *) data2);
        if (f1 > f2) return 1;
        else if (f1 < f2) return -1;
        else return 0;
    } else if (attrType == TypeInt) {
        int i1 = *((int *) data1), i2 = *((int *) data2);
        return i1 - i2;
    } else if (attrType == TypeVarChar) {
        int l1, l2;
        memcpy(&l1, data1, 4);
        memcpy(&l2, data2, 4);
        if (l1 == l2) {
            return memcmp((char *) data1 + 4, (char *) data2 + 4, l1);
        } else if (l1 > l2) {
            int res = memcmp((char *) data1 + 4, (char *) data2 + 4, l2);
            return res == 0 ? 1 : res;
        } else {
            int res = memcmp((char *) data1 + 4, (char *) data2 + 4, l1);
            return res == 0 ? -1 : res;
        }
    } else {
        throw std::invalid_argument("invalid attr type");
    }
}

void NodePage::deleteKeysStartFrom(const KeyNumber &keyIndex) {
    if (keyIndex >= slotNumber) {
        throw std::invalid_argument("key index exceeds range.");
    }
    auto firstDeleteKeyOffset = getNthKeyOffset(keyIndex);
    auto keysEndOffset = getFreeSpaceOffset();
    freeSpace += (keysEndOffset - firstDeleteKeyOffset) + ((sizeof(PageOffset) * (slotNumber - keyIndex)));
    slotNumber = keyIndex;
}

void * NodePage::getNthKey(const KeyNumber &keyIndex, const AttrType &attrType) {
    if (keyIndex >= slotNumber) {
        throw std::invalid_argument("key index exceeds range.");
    }
    auto keyOffset = getNthKeyOffset(keyIndex);
    auto length = getKeyLength((char *) pageData + keyOffset, attrType);

    void *keyData = malloc(length);
    if (keyData == nullptr) throw std::bad_alloc();

    memcpy(keyData, (char *) pageData + keyOffset, length);

    return keyData;
}

void * NodePage::copyToEnd(const KeyNumber &startKey, PageOffset &dataLength,
                           PageOffset &slotDataLength, KeyNumber &keyNumbers) {
    if (startKey >= slotNumber) {
        throw std::invalid_argument("key index exceeds range.");
    }

    // compute block length
    auto firstKeyOffset = getNthKeyOffset(startKey);
    auto keysEndOffset = getFreeSpaceOffset();
    dataLength = keysEndOffset - firstKeyOffset;
    auto firstKeySlotEndOffset = getNthSlotOffset(startKey) + sizeof(PageOffset);
    auto lastKeySlotStartOffset = getNthSlotOffset(slotNumber - 1);
    slotDataLength = firstKeySlotEndOffset - lastKeySlotStartOffset;

    void *block = malloc(dataLength + slotDataLength);
    if (block == nullptr) throw std::bad_alloc();

    memcpy(block, (char *) pageData + firstKeyOffset, dataLength);
    memcpy((char *) block + dataLength, (char *) pageData + lastKeySlotStartOffset, slotDataLength);
    keyNumbers = slotNumber - startKey;

    return block;
}


// =====================================================================================
//                                  Key Node Page
// =====================================================================================

KeyNodePage::KeyNodePage(void *pageData, bool init): NodePage(pageData, init) {
    if (init) {
        isLeaf = false;
        // write to pageData
        PageOffset offset = PAGE_SIZE - sizeof(bool);
        memcpy((char *) pageData + offset, &isLeaf, sizeof(bool));
    }
}

KeyNodePage::KeyNodePage(void *pageData, const void *block, const KeyNumber &keyNumbers, const PageOffset &dataLength,
                         const PageOffset &slotDataLength, const PageNum &prevKeyNodePageID): KeyNodePage(pageData, true) {
    // init first pointer
    int invalidP = -1;
    memcpy(pageData, &invalidP, sizeof(PageNum));
    // copy data
    memcpy((char *) pageData + sizeof(PageNum), block, dataLength);
    PageOffset slotDirOffset = PAGE_SIZE - infoSectionLength - slotDataLength;
    memcpy((char *) pageData + slotDirOffset, (char *) block + dataLength, slotDataLength);

    // assign info variables
    isLeaf = false;
    slotNumber = keyNumbers;
    freeSpace = PAGE_SIZE - dataLength - slotDataLength - infoSectionLength - sizeof(PageNum);

    // update slots offset
    auto deviateOffset = getNthKeyOffset(0) - sizeof(PageNum);
    updateSlots(0, slotNumber - 1, deviateOffset, false);

    // write info variables to page data
    PageOffset offset = PAGE_SIZE - sizeof(bool);
    memcpy((char *) pageData + offset, &isLeaf, sizeof(bool));
    offset -= sizeof(SlotNumber);
    memcpy((char *) pageData + offset, &slotNumber, sizeof(SlotNumber));
    offset -= sizeof(PageFreeSpace);
    memcpy((char *) pageData + offset, &freeSpace, sizeof(PageFreeSpace));
}

void KeyNodePage::addKey(const void *key, const AttrType &attrType, KeyNumber &keyIndex) {
    bool find = findKey(key, attrType, keyIndex);
    if (find) {
        throw std::invalid_argument("key already exists");
    }

    PageOffset keyLength = getKeyLength(key, attrType);

    PageOffset insertOffset;
    PageOffset insertLength = keyLength + sizeof(PageNum);
    if (keyIndex < slotNumber) {  // insert into middle
        // new key's inserting offset
        insertOffset = getNthKeyOffset(keyIndex);
        // move existed keys
        auto toMoveStartOffset = getNthKeyOffset(keyIndex);
        auto toMoveEndOffset = getFreeSpaceOffset();
        auto targetOffset = toMoveStartOffset + insertLength;
        moveData(toMoveStartOffset, targetOffset, toMoveEndOffset - toMoveStartOffset);
        // move existed slots
        toMoveStartOffset = getNthSlotOffset(slotNumber - 1);
        if (keyIndex == 0) {  // prevent number overflow, because KeyNumber is always positive
            toMoveEndOffset = PAGE_SIZE - infoSectionLength;
        } else {
            toMoveEndOffset = getNthSlotOffset(keyIndex - 1);
        }
        targetOffset = toMoveStartOffset - sizeof(PageOffset);
        moveData(toMoveStartOffset, targetOffset, toMoveEndOffset - toMoveStartOffset);
    } else {  // append a key
        if (slotNumber == 0) {
            // append the first left pointer
            int invalidP = -1;
            memcpy(pageData, &invalidP, sizeof(PageNum));
            freeSpace -= sizeof(PageNum);
        }
        // new key's inserting offset
        insertOffset = getFreeSpaceOffset();
    }

    // update freeSpace and slotNumber
    freeSpace -= (sizeof(PageOffset) + insertLength);  // new slot, key, new pointer
    slotNumber++;

    // write new key's slot
    writeNthSlot(keyIndex, insertOffset);

    // write key and new pointer
    memcpy((char *) pageData + insertOffset, key, keyLength);
    int invalidP = -1;
    memcpy((char *) pageData + insertOffset + keyLength, &invalidP, sizeof(PageNum));

    // not appended key, need to update moved slots
    if (keyIndex != slotNumber - 1) {
        updateSlots(keyIndex + 1, slotNumber - 1, insertLength, true);
    }

}

void KeyNodePage::setLeftPointer(const KeyNumber &keyIndex, const PageNum &pageID) {
    if (keyIndex >= slotNumber) {
        throw std::invalid_argument("key index exceeds rage");
    }
    auto keyOffset = getNthKeyOffset(keyIndex);
    auto pointerOffset = keyOffset - sizeof(PageNum);
    memcpy((char *) pageData + pointerOffset, &pageID, sizeof(PageNum));
}

void KeyNodePage::setRightPointer(const KeyNumber &keyIndex, const PageNum &pageID) {
    if (keyIndex >= slotNumber) {
        throw std::invalid_argument("key index exceeds rage");
    }
    auto keyOffset = getNthKeyOffset(keyIndex);
    auto pointerOffset = keyOffset + sizeof(PageNum);
    memcpy((char *) pageData + pointerOffset, &pageID, sizeof(PageNum));
}

PageNum KeyNodePage::getLeftPointer(const KeyNumber &keyIndex) {
    if (keyIndex >= slotNumber) {
        throw std::invalid_argument("key index exceeds rage");
    }
    auto keyOffset = getNthKeyOffset(keyIndex);
    auto pointerOffset = keyOffset - sizeof(PageNum);
    PageNum pointer;
    memcpy(&pointer, (char *) pageData + pointerOffset, sizeof(PageNum));
    return pointer;
}

PageNum KeyNodePage::getRightPointer(const KeyNumber &keyIndex) {
    if (keyIndex >= slotNumber) {
        throw std::invalid_argument("key index exceeds rage");
    }
    auto keyOffset = getNthKeyOffset(keyIndex);
    auto pointerOffset = keyOffset + sizeof(PageNum);
    PageNum pointer;
    memcpy(&pointer, (char *) pageData + pointerOffset, sizeof(PageNum));
    return pointer;
}


// =====================================================================================
//                                  Leaf Node Page
// =====================================================================================

LeafNodePage::LeafNodePage(void *pageData, bool init): NodePage(pageData, init) {
    infoSectionLength += sizeof(PageNum);
    if (init) {
        isLeaf = true;
        // write to pageData
        PageOffset offset = PAGE_SIZE - sizeof(bool);
        memcpy((char *) pageData + offset, &isLeaf, sizeof(bool));
        nextLeafPage = 0;
        memcpy((char *) pageData + PAGE_SIZE - infoSectionLength, &nextLeafPage, sizeof(PageNum));
        // update free space
        freeSpace = PAGE_SIZE - infoSectionLength;
    } else {
        PageOffset offset = PAGE_SIZE - infoSectionLength;
        memcpy(&nextLeafPage, (char *) pageData + offset, sizeof(PageNum));
    }
}

LeafNodePage::LeafNodePage(void *pageData, const void *block, const KeyNumber &keyNumbers, const PageOffset &dataLength,
                           const PageOffset &slotDataLength): LeafNodePage(pageData, true) {
    // copy data
    memcpy(pageData, block, dataLength);
    PageOffset slotDirOffset = PAGE_SIZE - infoSectionLength - slotDataLength;
    memcpy((char *) pageData + slotDirOffset, (char *) block + dataLength, slotDataLength);

    // assign info variables
    slotNumber = keyNumbers;
    freeSpace = PAGE_SIZE - dataLength - slotDataLength - infoSectionLength;
    isLeaf = true;

    // update slots offset
    auto deviateOffset = getNthKeyOffset(0);
    updateSlots(0, slotNumber - 1, deviateOffset, false);

    // write info variables to page data
    PageOffset offset = PAGE_SIZE - sizeof(bool);
    memcpy((char *) pageData + offset, &isLeaf, sizeof(bool));
    offset -= sizeof(SlotNumber);
    memcpy((char *) pageData + offset, &slotNumber, sizeof(SlotNumber));
    offset -= sizeof(PageFreeSpace);
    memcpy((char *) pageData + offset, &freeSpace, sizeof(PageFreeSpace));
    offset -= sizeof(PageNum);
    int invalidP = -1;
    memcpy((char *) pageData + offset, &invalidP, sizeof(PageNum));
}

PageOffset LeafNodePage::findRid(const KeyNumber &keyIndex, const AttrType &attrType, const RID &rid) {
    void *key = getNthKey(keyIndex, attrType);
    PageOffset keyOffset = getNthKeyOffset(keyIndex);
    PageOffset ridsOffset = keyOffset + getKeyLength(key, attrType);
    PageOffset ridsEndOffset;
    free(key);
    if (keyIndex == slotNumber - 1) {
        ridsEndOffset = getFreeSpaceOffset();
    } else {
        ridsEndOffset = getNthKeyOffset(keyIndex + 1);
    }
    assert((ridsEndOffset - ridsOffset) % sizeof(RID) == 0);

    // check whether rid already exists
    // TODO: can improve from O(N) to O(lgN)
    for (auto offset = ridsOffset; offset < ridsEndOffset; offset += sizeof(RID)) {
        RID tmpRid;
        memcpy(&tmpRid, (char *) pageData + offset, sizeof(RID));
        if (tmpRid.pageNum == rid.pageNum && tmpRid.slotNum == rid.slotNum) {
            // existed rid
            return offset;
        }
    }

    // not exist
    return 0;
}

void * LeafNodePage::getRIDs(const KeyNumber &keyIndex, PageOffset &dataLength, const AttrType &attrType) {
    void *key = getNthKey(keyIndex, attrType);
    PageOffset keyLength = getKeyLength(key, attrType);
    PageOffset ridsStartOffset = getNthKeyOffset(keyIndex) + keyLength;
    PageOffset ridsEndOffset;
    if (keyIndex == slotNumber - 1) {
        ridsEndOffset = getFreeSpaceOffset();
    } else {
        ridsEndOffset = getNthKeyOffset(keyIndex + 1);
    }
    free(key);

    dataLength = ridsEndOffset - ridsStartOffset;

    void *rids = malloc(dataLength);
    if (rids == nullptr) throw std::bad_alloc();
    memcpy(rids, (char *) pageData + ridsStartOffset, dataLength);

    return rids;
}

RC LeafNodePage::addKey(const void *key, const AttrType &attrType, const RID &rid) {
    // find key index
    KeyNumber keyIndex;
    bool find = findKey(key, attrType, keyIndex);

    PageOffset keyLength = getKeyLength(key, attrType);

    // existed key
    if (find) {
        if (findRid(keyIndex, attrType, rid) != 0) {
            // existed rid
            return 1;
        }

        // rid not exist, insert new rid to end
        if (keyIndex == slotNumber - 1) {  // append to free space start
            memcpy((char *) pageData + getFreeSpaceOffset(), &rid, sizeof(RID));
            freeSpace -= sizeof(RID);
        } else {
            // move later keys
            auto moveStartOffset = getNthKeyOffset(keyIndex + 1);
            auto moveTargetOffset = moveStartOffset + sizeof(RID);
            moveData(moveStartOffset, moveTargetOffset, getFreeSpaceOffset() - moveStartOffset);
            memcpy((char *) pageData + moveStartOffset, &rid, sizeof(RID));
            freeSpace -= sizeof(RID);
            // update slots
            updateSlots(keyIndex + 1, slotNumber - 1, sizeof(RID), true);
        }
    }

    // key not exist
    // TODO: write below code into a function
    else {
        // move key
        PageOffset insertOffset;
        PageOffset insertLength = keyLength + sizeof(RID);
        if (keyIndex < slotNumber) {  // insert into existed keys
            // new key's inserting offset
            insertOffset = getNthKeyOffset(keyIndex);
            // move existed keys
            auto toMoveStartOffset = getNthKeyOffset(keyIndex);
            auto toMoveEndOffset = getFreeSpaceOffset();
            auto targetOffset = toMoveStartOffset + insertLength;
            moveData(toMoveStartOffset, targetOffset, toMoveEndOffset - toMoveStartOffset);
            // move existed slots
            toMoveStartOffset = getNthSlotOffset(slotNumber - 1);
            if (keyIndex == 0) {  // prevent number overflow, because KeyNumber is always positive
                toMoveEndOffset = PAGE_SIZE - infoSectionLength;
            } else {
                toMoveEndOffset = getNthSlotOffset(keyIndex - 1);
            }
            targetOffset = toMoveStartOffset - sizeof(PageOffset);
            moveData(toMoveStartOffset, targetOffset, toMoveEndOffset - toMoveStartOffset);
        } else {  // append a key
            // new key's inserting offset
            insertOffset = getFreeSpaceOffset();
        }

        // update freeSpace and slotNumber
        freeSpace -= (sizeof(PageOffset) + insertLength);  // new slot, key, rid
        slotNumber++;

        // write new key's slot
        writeNthSlot(keyIndex, insertOffset);

        // write key and rid
        memcpy((char *) pageData + insertOffset, key, keyLength);
        memcpy((char *) pageData + insertOffset + keyLength, &rid, sizeof(RID));

        // not appended key, need to update moved slots
        if (keyIndex != slotNumber - 1) {
            updateSlots(keyIndex + 1, slotNumber - 1, insertLength, true);
        }
    }

    return 0;
}

RC LeafNodePage::deleteKey(const void *key, const AttrType &attrType, const RID &rid) {
    KeyNumber keyIndex;
    bool find = findKey(key, attrType, keyIndex);
    if (!find) {  // key not exist
        return 1;
    }

    auto ridOffset = findRid(keyIndex, attrType, rid);
    if (ridOffset == 0) {  // rid not exist
        return 1;
    }
    PageOffset ridsEndOffset;
    if (keyIndex == slotNumber - 1) {
        ridsEndOffset = getFreeSpaceOffset();
    } else {
        ridsEndOffset = getNthKeyOffset(keyIndex + 1);
    }

    PageOffset keyOffset = getNthKeyOffset(keyIndex);
    PageOffset keyLength = getKeyLength(key, attrType);
    PageOffset deleteLength;

    // key only has one rid
    if (keyOffset + keyLength + sizeof(RID) == ridsEndOffset) {

        deleteLength = keyLength + sizeof(RID) + sizeof(PageOffset);

        // last key
        if (keyIndex == slotNumber - 1) {
            // update freeSpace and slotNumber
            freeSpace += deleteLength;
            slotNumber--;
        }

        // middle key
        else {
            // move later key data
            PageOffset moveStartOffset = getNthKeyOffset(keyIndex + 1);
            moveData(moveStartOffset, keyOffset, getFreeSpaceOffset() - moveStartOffset);
            // move later slots
            moveStartOffset = getNthSlotOffset(slotNumber - 1);
            PageOffset moveTargetOffset = moveStartOffset + sizeof(PageOffset);
            moveData(moveStartOffset, moveTargetOffset, getNthSlotOffset(keyIndex) - moveStartOffset);
            // update freeSpace and slotNumber
            freeSpace += deleteLength;
            slotNumber--;
            // update slots
            // now keyIndex points to the first later slot
            updateSlots(keyIndex, slotNumber - 1, keyLength + sizeof(RID), false);
        }

    }

    // key has more than one rid
    else {

        deleteLength = sizeof(RID);

        // move later rids
        PageOffset moveStartOffset = ridOffset + sizeof(RID);
        PageOffset moveTargetOffset = ridOffset;
        PageOffset moveLength = getFreeSpaceOffset() - moveStartOffset;
        moveData(moveStartOffset, moveTargetOffset, moveLength);

        // update freeSpace
        freeSpace += deleteLength;

        // not the last key, need to update slots
        if (keyIndex != slotNumber - 1) {
            updateSlots(keyIndex + 1, slotNumber - 1, deleteLength, false);
        }
    }

    return 0;
}

RC LeafNodePage::getNextLeafPageID(PageNum &nextPageID) {
    if (nextLeafPage == 0) return -1;
    return nextLeafPage;
}

void LeafNodePage::setNextLeafPageID(const PageNum &nextPageID) {
    nextLeafPage = nextPageID;
    auto offset = PAGE_SIZE - infoSectionLength;
    memcpy((char *) pageData + offset, &nextPageID, sizeof(PageNum));
}