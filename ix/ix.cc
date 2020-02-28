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

std::string IndexManager::getString(const void *key) {
    int length;
    memcpy(&length, (char *) key, sizeof(int));
    std::string str;
    for (int i = 0; i < length; i++) {
        str += *((char *) key + sizeof(int) + i);
    }
    return str;
}

RC IndexManager::insertEntry(IXFileHandle &ixFileHandle, const Attribute &attribute, const void *key, const RID &rid) {
    RC rc;
    PageNum root = ixFileHandle.getRootNodeID();
    if (root == IXFileHandle::NotExistRootPageID) {
        void *pageData = malloc(PAGE_SIZE);
        if (pageData == nullptr) throw std::bad_alloc();
        LeafNodePage nodePage(pageData, true);
        ixFileHandle.appendNodePage(nodePage.getPageData(), root);
        ixFileHandle.setRootNodeID(root);
        free(pageData);
    }
    PageNum returnedPointer = root;
    void* returnedKey = malloc(PAGE_SIZE);
    int hasReturned = 0;
    memcpy((char *) returnedKey + PAGE_SIZE - sizeof(int), &hasReturned, sizeof(int));
    rc = insertEntry(ixFileHandle, attribute, key, rid, root, returnedPointer, returnedKey);
    free(returnedKey);
    return rc;
}

RC IndexManager::deleteEntry(IXFileHandle &ixFileHandle, const Attribute &attribute, const void *key, const RID &rid) {
    RC rc;
    PageNum root = ixFileHandle.getRootNodeID();
    rc = deleteEntry(ixFileHandle, attribute, key, rid, root);
    return rc;
}

RC IndexManager::scan(IXFileHandle &ixFileHandle,
                      const Attribute &attribute,
                      const void *lowKey,
                      const void *highKey,
                      bool lowKeyInclusive,
                      bool highKeyInclusive,
                      IX_ScanIterator &ix_ScanIterator) {
    if (!ixFileHandle.isOccupied()) return -1;
    ix_ScanIterator.setup(ixFileHandle, attribute, lowKey, highKey, lowKeyInclusive, highKeyInclusive);
    return 0;
}

void IndexManager::printBtree(IXFileHandle &ixFileHandle, const Attribute &attribute) const {
    PageNum root = ixFileHandle.getRootNodeID();
    printBTree(ixFileHandle, attribute, root, 0);
}

RC IndexManager::insertEntry(IXFileHandle &ixFileHandle, const Attribute &attribute, const void *key, const RID &rid,
                             PageNum cur, PageNum &returnedPointer, void *returnedKey) {
    RC rc = 0;
    void* page = malloc(PAGE_SIZE);
    ixFileHandle.readNodePage(page, cur);
    bool isLeafNode = NodePage::isLeafNode(page);

    if (isLeafNode) { // leafNode
        LeafNodePage node(page, false);

        if (node.hasEnoughSpace(attribute, key)) {
            rc = node.addKey(key, attribute.type, rid);
            if (rc == 0) {
                ixFileHandle.writeNodePage(node.getPageData(), cur);
            }
        } else {
            if (node.getSlotNumber() == 1) {
                throw std::invalid_argument("page is already full with the same key");
            }

            // split
            rc = splitLeafNode(ixFileHandle, attribute, cur, key, rid, returnedKey, returnedPointer);

            // if current page is root, create new root
            if (cur == ixFileHandle.getRootNodeID()) {
                PageNum newRootid;
                void* data = malloc(PAGE_SIZE);
                KeyNodePage newRoot(data);

                KeyNumber keyIndex;
                newRoot.addKey(returnedKey, attribute.type, keyIndex);
                newRoot.setRightPointer(0, returnedPointer, attribute.type);
                newRoot.setLeftPointer(0, cur);
                ixFileHandle.appendNodePage(data, newRootid);
                ixFileHandle.setRootNodeID(newRootid);
                free(data);
            }
        }
    } else { // keyNode
        KeyNodePage node(page, false);
        PageNum targetNode = findNextNode(ixFileHandle, attribute, cur, key);
        rc = insertEntry(ixFileHandle, attribute, key, rid, targetNode, returnedPointer, returnedKey);

        int hasReturn;
        memcpy(&hasReturn, (char *) returnedKey + PAGE_SIZE - sizeof(int), sizeof(int));
        // no split before;
        if (hasReturn == 0) return 0;
        // split detected, add returned entry
        if (node.hasEnoughSpace(attribute, returnedKey)) {
            KeyNumber keyIndex;
            node.addKey(returnedKey, attribute.type, keyIndex);
            node.setRightPointer(keyIndex, returnedPointer, attribute.type);
            hasReturn = 0;
            memcpy((char *) returnedKey + PAGE_SIZE - sizeof(int), &hasReturn, sizeof(int));
            ixFileHandle.writeNodePage(node.getPageData(), cur);
        } else { // no enough space
            // split
            splitKeyNode(ixFileHandle, attribute, cur, returnedKey, returnedPointer);

            // if current page is root, create new root
            if (cur == ixFileHandle.getRootNodeID()) {
                PageNum newRootid;
                void* data = malloc(PAGE_SIZE);
                KeyNodePage newRoot(data);

                KeyNumber keyIndex;
                newRoot.addKey(returnedKey, attribute.type, keyIndex);
                newRoot.setRightPointer(0, returnedPointer, attribute.type);
                newRoot.setLeftPointer(0, cur);
                ixFileHandle.appendNodePage(data, newRootid);
                ixFileHandle.setRootNodeID(newRootid);
                free(data);
            }
        }
    }
    free(page);
    return rc;
}

RC IndexManager::deleteEntry(IXFileHandle &ixFileHandle, const Attribute &attribute, const void *key, const RID &rid,
                             const PageNum &cur) {
    RC rc = 0;
    void* page = malloc(PAGE_SIZE);
    ixFileHandle.readNodePage(page, cur);
    bool isLeafNode = NodePage::isLeafNode(page);

    if (isLeafNode) { // reach leaf node, delete target rid
        LeafNodePage node(page, false);
        rc = node.deleteKey(key, attribute.type, rid);
        if (rc == 0) {
            ixFileHandle.writeNodePage(node.getPageData(), cur);
        }
    } else { // go through KeyNodes
        KeyNodePage node(page, false);
        PageNum targetNode = findNextNode(ixFileHandle, attribute, cur, key);
        rc = deleteEntry(ixFileHandle, attribute, key, rid, targetNode);
    }
    free(page);
    return rc;
}

void IndexManager::printBTree(IXFileHandle &ixFileHandle, const Attribute &attribute, PageNum &pageId,
                              int level) const {
    void* data = malloc(PAGE_SIZE);
    ixFileHandle.readNodePage(data, pageId);
    bool isLeaf = NodePage::isLeafNode(data);

    if (isLeaf) { // print leaf node in one line
        printSpace(level);
        std::cout << "{\"keys\": [";
        LeafNodePage node(data, false);
        SlotNumber slotNumber = node.getSlotNumber();
        void* tempKey;
        void* tempRID;

        PageOffset ridLength;
        // print each key and the following rids
        for (SlotNumber i = 0; i < slotNumber; i++) {
            tempKey = node.getNthKey(i, attribute.type);
            std::cout << "\"";
            printKey(attribute, tempKey);
            std::cout << ":[";
            tempRID = node.getRIDs(i, ridLength, attribute.type);
            printRids(tempRID, ridLength);
            std::cout << "]\"";
            if (i < slotNumber - 1) {
                std::cout << ",";
            }
            free(tempKey);
            free(tempRID);
        }
        std::cout << "]}";
    } else { // print keyNode
        printSpace(level);
        std::cout << "{" << std::endl;
        printSpace(level);
        std::cout << "\"keys\": [";
        KeyNodePage node(data, false);
        void* tempKey;
        SlotNumber slotNumber = node.getSlotNumber();
        // print all keys;
        for (int i = 0; i < slotNumber; i++) {
            tempKey = node.getNthKey(i, attribute.type);
            std::cout << "\"";
            printKey(attribute, tempKey);
            std::cout << "\"";
            if (i < slotNumber - 1) {
                std::cout << ",";
            }
            free(tempKey);
        }
        std::cout << "]," << std::endl;
        printSpace(level);
        std::cout << "\"children\": [" << std::endl;
        PageNum pageNum;
        // print children
        pageNum = node.getLeftPointer(0);
        printBTree(ixFileHandle, attribute, pageNum, level + 1);
        if (slotNumber > 0) {
            std::cout << "," << std::endl;
        } else {
            std::cout << std::endl;
        }

        for (int i = 0; i < slotNumber; i++) {
            pageNum = node.getRightPointer(i, attribute.type);
            printBTree(ixFileHandle, attribute, pageNum, level + 1);
            if (i < slotNumber - 1) {
                std::cout << "," << std::endl;
            } else {
                std::cout << std::endl;
            }
        }
        printSpace(level);
        std::cout << "]" << std::endl;
        printSpace(level);
        std::cout << "}";
    }
}

void IndexManager::printSpace(int spaceNum) const {
    for (int i = 0; i < spaceNum; i++) {
        std::cout << " ";
    }
}

void IndexManager::printKey(const Attribute &attribute, const void *data) const {
    if (attribute.type == TypeVarChar) {
        std::string str = getString(data);
        std::cout << str;
    } else if (attribute.type == TypeInt) {
        int intKey;
        memcpy(&intKey, (char *) data, sizeof(int));
        std::cout << intKey;
    } else {
        float floatKey;
        memcpy(&floatKey, (char *) data, sizeof(float));
        std::cout << floatKey;
    }
}

void IndexManager::printRids(const void *data, PageOffset totalLength) const {
    PageOffset length = 0;
    //PageOffset ridLength = sizeof(PageNum) + sizeof(SlotNumber);
    RID rid;
    while (length < totalLength) {
        memcpy(&rid, (char *) data + length, sizeof(RID));
        std::cout << "(" << rid.pageNum << "," << rid.slotNum << ")";
        length += sizeof(RID);
        if (length < totalLength) {
            std::cout << ",";
        }
    }
}

PageNum IndexManager::findNextNode(IXFileHandle &ixFileHandle, const Attribute &attribute, const PageNum &cur,
                                   const void *key) {
    PageNum target;
    void* page = malloc(PAGE_SIZE);
    ixFileHandle.readNodePage(page, cur);
    KeyNodePage curNode(page, false);
    // initially, target is the left most pointer
    target = curNode.getLeftPointer(0);
    void* tempKey;
    for (int i = 0; i < curNode.getSlotNumber(); i++) {
        tempKey = curNode.getNthKey(i, attribute.type);
        if (curNode.compare(key, tempKey, attribute.type) >= 0) {
            target = curNode.getRightPointer(i, attribute.type);
        } else { // target found stop
            free(tempKey);
            break;
        }
        free(tempKey);
    }
    free(page);
    return target;
}

void IndexManager::splitKeyNode(IXFileHandle &ixFileHandle, const Attribute &attribute, const PageNum id,
                                void* returnedKey, PageNum &returnedPointer) {
    // create new page, move half entries to the new one;
    void* curPage = malloc(PAGE_SIZE);
    void* newPage = malloc(PAGE_SIZE);
    ixFileHandle.readNodePage(curPage, id);
    KeyNodePage curNode(curPage, false);
    int totalSlotNumber = curNode.getSlotNumber();

    // splitPosition here is slotNumber, splitPosition - 1 is the actual keyIndex;
    // start from splitPosition, the first one is pushed up and will not be moved to new Page
    int splitPosition = (totalSlotNumber + 1) / 2;
    PageOffset dataLength, slotDataLength;
    KeyNumber keyNumber;
    void* movedBlock = curNode.copyToEnd(splitPosition, dataLength, slotDataLength, keyNumber);
    KeyNodePage newNode(newPage, movedBlock, keyNumber, dataLength, slotDataLength);

//    void* removedKey = malloc(PAGE_SIZE);
//    void* tempKey;
//    PageNum rightPointer;
//    AttrLength length;
//    for (int i = splitPosition; i < totalSlotNumber; i++) {
//        if (attribute.type == TypeVarChar) {
//            length = 0;
//            tempKey = curNode.getNthKey(i, length);
//            memcpy((char *) removedKey, &length, sizeof(AttrLength));
//            memcpy((char *) removedKey + sizeof(AttrLength), (char *)tempKey, length);
//        } else {
//            length = 4;
//            tempKey = curNode.getNthKey(i, length);
//            memcpy((char *) removedKey, (char *) tempKey, sizeof(int));
//        }
//        rightPointer = curNode.getRightPointer(i);
//        newNode.addNewEntry(attribute, removedKey, rightPointer);
//    }

    // set left most pointer of new Page
    PageNum left = curNode.getLeftPointer(splitPosition);
    newNode.setLeftPointer(0, left);

    // get middleKey
    void* middle;
    middle = curNode.getNthKey(splitPosition - 1, attribute.type);
    //delete half keys
    curNode.deleteKeysStartFrom(splitPosition - 1);

    // add new key
    KeyNumber keyIndex;
    if (curNode.compare(returnedKey, middle, attribute.type) < 0) {
        curNode.addKey(returnedKey, attribute.type, keyIndex);
        curNode.setRightPointer(keyIndex, returnedPointer, attribute.type);
    } else {
        newNode.addKey(returnedKey, attribute.type, keyIndex);
        newNode.setRightPointer(keyIndex, returnedPointer, attribute.type);
    }

    // set middleKey
    if (attribute.type == TypeVarChar) {
        PageOffset length = NodePage::getKeyLength(middle, attribute.type);
        memcpy((char *) returnedKey, (char *) middle, length);
    } else {
        memcpy((char *) returnedKey, (char *) middle, sizeof(int));
    }

    ixFileHandle.writeNodePage(curNode.getPageData(), id);
    ixFileHandle.appendNodePage(newNode.getPageData(), returnedPointer);
    free(curPage);
    free(newPage);
    free(movedBlock);
    free(middle);
}

RC IndexManager::splitLeafNode(IXFileHandle &ixFileHandle, const Attribute &attribute, const PageNum id, const void* key,
                                 const RID &rid, void *returnedKey, PageNum &returnedPointer) {
    RC rc;
    // create new page, move half entries to the new one;
    void* curPage = malloc(PAGE_SIZE);
    void* newPage = malloc(PAGE_SIZE);
    ixFileHandle.readNodePage(curPage, id);
    LeafNodePage curNode(curPage, false);
    int totalSlotNumber = curNode.getSlotNumber();
    // splitPosition here is slotNumber, splitPosition - 1 is the actual keyIndex;
    // start from splitPosition, all keys will be moved to the new page
    int splitPosition = (totalSlotNumber + 1) / 2 + 1;
    PageOffset dataLength, slotDataLength;
    KeyNumber keyNumber;
    void* movedBlock = curNode.copyToEnd(splitPosition - 1, dataLength, slotDataLength, keyNumber);
    LeafNodePage newNode(newPage, movedBlock, keyNumber, dataLength, slotDataLength);

    // set middleKey
    void* middle = curNode.getNthKey(splitPosition - 1, attribute.type);
    if (attribute.type == TypeVarChar) {
        PageOffset length = NodePage::getKeyLength(middle, attribute.type);
        memcpy((char *) returnedKey, (char *) middle, length);
    } else {
        memcpy((char *) returnedKey, (char *) middle, sizeof(int));
    }
    int hasReturn = 1;
    memcpy((char *) returnedKey + PAGE_SIZE - sizeof(int), &hasReturn, sizeof(int));

//    void* removedKey = malloc(PAGE_SIZE);
//    void* tempKey;
//    void* ridPointer;
//    RID rid;
//    int ridLength;
//    AttrLength length;
//    for (int i = splitPosition; i < totalSlotNumber; i++) {
//        if (attribute.type == TypeVarChar) {
//            length = 0;
//            tempKey = curNode.getNthKey(i, length);
//            memcpy((char *) removedKey, &length, sizeof(AttrLength));
//            memcpy((char *) removedKey + sizeof(AttrLength), (char *)tempKey, length);
//        } else {
//            length = 4;
//            tempKey = curNode.getNthKey(i, length);
//            memcpy((char *) removedKey, (char *) tempKey, sizeof(int));
//        }
//
//        if (i == splitPosition) {
//            memcpy((char *) middleKey, (char *) removedKey, sizeof(int) + length);
//        }
//
//        ridPointer = curNode.getRIDs(i, ridLength);
//        for (int j = 0; j < ridLength / sizeof(RID); j++) {
//            memcpy(&rid, (char *)ridPointer + j * sizeof(RID), sizeof(RID));
//            newNode.addNewEntry(attribute, removedKey, rid);
//        }
//    }

    // delete half entries
    curNode.deleteKeysStartFrom(splitPosition - 1);

    // add new key
    if (curNode.compare(key, returnedKey, attribute.type) < 0) {
        rc = curNode.addKey(key, attribute.type, rid);
    } else {
        rc = newNode.addKey(key, attribute.type, rid);
    }

    // set next Pointer
    PageNum nextLeafPointer;
    curNode.getNextLeafPageID(nextLeafPointer);
    curNode.setNextLeafPageID(ixFileHandle.totalPageNum);
    newNode.setNextLeafPageID(nextLeafPointer);

    ixFileHandle.writeNodePage(curNode.getPageData(), id);
    ixFileHandle.appendNodePage(newNode.getPageData(), returnedPointer);


    free(curPage);
    free(newPage);
    free(movedBlock);
    free(middle);
    return rc;
}

IX_ScanIterator::IX_ScanIterator() {
    currentPage = NodePage::NotExistPointer;
    currentPageData = nullptr;
    currentKey = nullptr;
    lastRIDIndex = NotStartRIDIndex;
    currentKeyIndex = NotStartKeyIndex;
    currentKeyRIDNumber = 0;
    lowKey = highKey = nullptr;
    ixFileHandle = nullptr;
}

IX_ScanIterator::~IX_ScanIterator() {
}

void IX_ScanIterator::setup(IXFileHandle &ixFileHandle,
           const Attribute &attribute,
           const void* lowKey, const void* highKey,
           const bool &lowKeyInclusive, const bool &highKeyInclusive) {
    if (this->ixFileHandle != nullptr || this->currentKey != nullptr
        || this->lowKey != nullptr || this->highKey != nullptr) {
        // last scan not close
        std::cerr << "last scan not close!" << std::endl;
        throw std::bad_function_call();
    }
    this->attribute = attribute;
    currentPageData = malloc(PAGE_SIZE);
    if (currentPageData == nullptr) throw std::bad_alloc();
    this->ixFileHandle = &ixFileHandle;
    if (lowKey != nullptr) {
        PageOffset keyLength = NodePage::getKeyLength(lowKey, attribute.type);
        this->lowKey = malloc(keyLength);
        if (this->lowKey == nullptr) throw std::bad_alloc();
        memcpy(this->lowKey, lowKey, keyLength);
    }
    if (highKey != nullptr) {
        PageOffset keyLength = NodePage::getKeyLength(highKey, attribute.type);
        this->highKey = malloc(keyLength);
        if (this->highKey == nullptr) throw std::bad_alloc();
        memcpy(this->highKey, highKey, keyLength);
    }
    this->lowKeyInclusive = lowKeyInclusive;
    this->highKeyInclusive = highKeyInclusive;

    // point currentKey to the first key in leftmost leaf node
    currentPage = ixFileHandle.getRootNodeID();
    if (currentPage == IXFileHandle::NotExistRootPageID) {
        // no page in current index
        currentKey = nullptr;
    } else {
        // reach left most leaf page
        while (true) {
            ixFileHandle.readNodePage(currentPageData, currentPage);
            if (NodePage::isLeafNode(currentPageData)) {
                break;
            } else {
                KeyNodePage nodePage(currentPageData, false);
                currentPage = nodePage.getLeftPointer(0);
                assert(currentPage != NodePage::NotExistPointer);
            }
        }
        // find the first satisfied key
        findNextKey();
    }
}

bool IX_ScanIterator::findNextKey() {
    while (true) {
        assert(NodePage::isLeafNode(currentPageData));
        LeafNodePage nodePage(currentPageData, false);

        // empty leaf node or reach current leaf's end, read next leaf node
        if (nodePage.getSlotNumber() == 0 ||
            (currentKeyIndex != NotStartKeyIndex
             && currentKeyIndex >= nodePage.getSlotNumber() - 1 )) {
            nodePage.getNextLeafPageID(currentPage);
            if (currentPage == NodePage::NotExistPointer) {
                // iterated whole file, no key in current index
                currentKey = nullptr;
                break;
            } else {
                // read next leaf node data
                ixFileHandle->readNodePage(currentPageData, currentPage);
                currentKeyIndex = NotStartKeyIndex;
                lastRIDIndex = NotStartRIDIndex;
            }
        }

        // read current leaf's next key
        else {
            if (currentKeyIndex == NotStartKeyIndex) {
                currentKeyIndex = 0;
            } else {
                currentKeyIndex++;
            }
            currentKey = nodePage.getNthKey(currentKeyIndex, attribute.type);
            if (!currentKeySatisfied()) {
                free(currentKey);
                currentKey = nullptr;
            } else {
                // find satisfied key
                currentKeyRIDNumber = nodePage.getRIDNumber(currentKeyIndex, attribute.type);
                lastRIDIndex = NotStartRIDIndex;
                break;
            }
        }
    }
    return currentKey == nullptr;
}

bool IX_ScanIterator::currentKeySatisfied() {
    assert(currentKey != nullptr);
    if (lowKey != nullptr) {
        RC res = NodePage::compare(lowKey, currentKey, attribute.type);
        if (res > 0) return false;
        if (res == 0 && !lowKeyInclusive) return false;
    }
    if (highKey != nullptr) {
        RC res = NodePage::compare(highKey, currentKey, attribute.type);
        if (res < 0) return false;
        if (res == 0 && !highKeyInclusive) return false;
    }
    return true;
}

RC IX_ScanIterator::getNextEntry(RID &rid, void *key) {
    if (currentKey == nullptr) {
        return IX_EOF;
    }

    LeafNodePage nodePage(currentPageData, false);
    KeyNumber nextRIDIndex;

    // check if current key still exists
    if (currentKeyIndex == nodePage.getSlotNumber()) {
        // key has been deleted
        free(currentKey);
        findNextKey();
        if (currentKey == nullptr) {
            return IX_EOF;
        }
    } else {
        void *tmpKey = nodePage.getNthKey(currentKeyIndex, attribute.type);
        RC res = NodePage::compare(tmpKey, currentKey, attribute.type);
        free(tmpKey);
        if (res != 0) {
            // key has been deleted
            free(currentKey);
            findNextKey();
            if (currentKey == nullptr) {
                return IX_EOF;
            }
        }
    }

    // check if last rid has been deleted
    KeyNumber ridNumber = nodePage.getRIDNumber(currentKeyIndex, attribute.type);
    if (ridNumber != currentKeyRIDNumber) {
        nextRIDIndex = lastRIDIndex;
        currentKeyRIDNumber = ridNumber;
    } else {
        nextRIDIndex = lastRIDIndex + 1;
    }

    // check if current key has unread rid
    if (nextRIDIndex == currentKeyRIDNumber) {
        free(currentKey);
        findNextKey();
        if (currentKey == nullptr) {
            return IX_EOF;
        }
        nextRIDIndex = 0;
    }

    PageOffset keyLength = NodePage::getKeyLength(currentKey, attribute.type);
    memcpy(key, currentKey, keyLength);
    RID *targetRID = nodePage.getRID(currentKeyIndex, nextRIDIndex, attribute.type);
    rid.pageNum = targetRID->pageNum;
    rid.slotNum = targetRID->slotNum;
    free(targetRID);
    lastRIDIndex = nextRIDIndex;

    return 0;
}

RC IX_ScanIterator::close() {
    if (currentPageData != nullptr) free(currentPageData);
    currentPageData = nullptr;
    if (currentKey != nullptr) free(currentKey);
    currentKey = nullptr;
    if (lowKey != nullptr) free(lowKey);
    lowKey = nullptr;
    if (highKey != nullptr) free(highKey);
    highKey = nullptr;
    currentKeyRIDNumber = 0;
    lastRIDIndex = NotStartRIDIndex;
    currentKeyIndex = NotStartKeyIndex;
    ixFileHandle = nullptr;
    currentPage = NodePage::NotExistPointer;
    return 0;
}

IXFileHandle::IXFileHandle() {
    ixReadPageCounter = 0;
    ixWritePageCounter = 0;
    ixAppendPageCounter = 0;
    totalPageNum = 1;  // hidden page
    handle = nullptr;
    rootPageID = NotExistRootPageID;
}

RC IXFileHandle::writeHiddenPage() {
    if (handle == nullptr) return 1;
    void *data = malloc(PAGE_SIZE);
    if (data == nullptr) throw std::bad_alloc();
    memset(data, 0, PAGE_SIZE);
    *((char *) data) = 'Y';
    PageOffset offset = sizeof(char);
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
    readNodePage(data, 0);
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
    if (readHiddenPage() != 0) {
        writeHiddenPage();
    } else {
        ixReadPageCounter++;  // count this read
    }
}

void IXFileHandle::releaseHandle() {
    ixWritePageCounter++;  // count this write
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
        readInfoSection();
    } else {
        // init
        this->pageData = pageData;
        slotNumber = 0;
        // note: need update freeSpace in LeafNodePage's constructor
        freeSpace = PAGE_SIZE - infoSectionLength;
        // write to pageData
        writeInfoSection();
    }
}

void NodePage::writeInfoSection() {
    PageOffset offset = PAGE_SIZE - sizeof(bool);
    memcpy((char *) pageData + offset, &isLeaf, sizeof(bool));
    offset -= sizeof(SlotNumber);
    memcpy((char *) pageData + offset, &slotNumber, sizeof(SlotNumber));
    offset -= sizeof(PageFreeSpace);
    memcpy((char *) pageData + offset, &freeSpace, sizeof(PageFreeSpace));
}

void NodePage::readInfoSection() {
    PageOffset offset = PAGE_SIZE - sizeof(bool);
    memcpy(&isLeaf, (char *) pageData + offset, sizeof(bool));
    offset -= sizeof(SlotNumber);
    memcpy(&slotNumber, (char *) pageData + offset, sizeof(SlotNumber));
    offset -= sizeof(PageFreeSpace);
    memcpy(&freeSpace, (char *) pageData + offset, sizeof(PageFreeSpace));
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
    writeInfoSection();
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
                         const PageOffset &slotDataLength): KeyNodePage(pageData, true) {
    // init first pointer
    memcpy(pageData, &NotExistPointer, sizeof(PageNum));
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
    writeInfoSection();
}

bool KeyNodePage::hasEnoughSpace(const Attribute &attribute, const void *key) {
    // add slot size
    int sizeNeeded = sizeof(PageOffset);

    // add key size
    if (attribute.type == TypeVarChar) {
        int length;
        memcpy(&length, (char *) key, sizeof(int));
        sizeNeeded += length + sizeof(int);
    } else {
        sizeNeeded += sizeof(int);
    }

    // add Pointer size;
    sizeNeeded += sizeof(PageNum);

    return sizeNeeded <= freeSpace;
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
            memcpy(pageData, &NotExistPointer, sizeof(PageNum));
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
    memcpy((char *) pageData + insertOffset + keyLength, &NotExistPointer, sizeof(PageNum));

    // not appended key, need to update moved slots
    if (keyIndex != slotNumber - 1) {
        updateSlots(keyIndex + 1, slotNumber - 1, insertLength, true);
    }

    writeInfoSection();

}

void KeyNodePage::setLeftPointer(const KeyNumber &keyIndex, const PageNum &pageID) {
    if (keyIndex >= slotNumber) {
        throw std::invalid_argument("key index exceeds rage");
    }
    auto keyOffset = getNthKeyOffset(keyIndex);
    auto pointerOffset = keyOffset - sizeof(PageNum);
    memcpy((char *) pageData + pointerOffset, &pageID, sizeof(PageNum));
}

void KeyNodePage::setRightPointer(const KeyNumber &keyIndex, const PageNum &pageID, const AttrType &attrType) {
    if (keyIndex >= slotNumber) {
        throw std::invalid_argument("key index exceeds rage");
    }
    auto keyOffset = getNthKeyOffset(keyIndex);
    void* key = getNthKey(keyIndex, attrType);
    auto pointerOffset = keyOffset + getKeyLength(key, attrType);
    memcpy((char *) pageData + pointerOffset, &pageID, sizeof(PageNum));
    free(key);
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

PageNum KeyNodePage::getRightPointer(const KeyNumber &keyIndex, const AttrType &attrType) {
    if (keyIndex >= slotNumber) {
        throw std::invalid_argument("key index exceeds rage");
    }
    auto keyOffset = getNthKeyOffset(keyIndex);
    void* key = getNthKey(keyIndex, attrType);
    auto pointerOffset = keyOffset + getKeyLength(key, attrType);
    PageNum pointer;
    memcpy(&pointer, (char *) pageData + pointerOffset, sizeof(PageNum));
    free(key);
    return pointer;
}


// =====================================================================================
//                                  Leaf Node Page
// =====================================================================================

LeafNodePage::LeafNodePage(void *pageData, bool init): NodePage(pageData, init) {
    infoSectionLength += sizeof(PageNum);
    if (init) {
        isLeaf = true;
        nextLeafPage = NotExistPointer;
        // update free space
        freeSpace = PAGE_SIZE - infoSectionLength;
        // write to pageData
        writeInfoSection();
    } else {
        readInfoSection();
    }
}

void LeafNodePage::readInfoSection() {
    PageOffset offset = PAGE_SIZE - sizeof(bool);
    memcpy(&isLeaf, (char *) pageData + offset, sizeof(bool));
    offset -= sizeof(SlotNumber);
    memcpy(&slotNumber, (char *) pageData + offset, sizeof(SlotNumber));
    offset -= sizeof(PageFreeSpace);
    memcpy(&freeSpace, (char *) pageData + offset, sizeof(PageFreeSpace));
    offset -= sizeof(PageNum);
    memcpy(&nextLeafPage, (char *) pageData + offset, sizeof(PageNum));
}

void LeafNodePage::writeInfoSection() {
    PageOffset offset = PAGE_SIZE - sizeof(bool);
    memcpy((char *) pageData + offset, &isLeaf, sizeof(bool));
    offset -= sizeof(SlotNumber);
    memcpy((char *) pageData + offset, &slotNumber, sizeof(SlotNumber));
    offset -= sizeof(PageFreeSpace);
    memcpy((char *) pageData + offset, &freeSpace, sizeof(PageFreeSpace));
    offset -= sizeof(PageNum);
    memcpy((char *) pageData + offset, &nextLeafPage, sizeof(PageNum));
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
    nextLeafPage = NotExistPointer;

    // update slots offset
    auto deviateOffset = getNthKeyOffset(0);
    updateSlots(0, slotNumber - 1, deviateOffset, false);

    // write info variables to page data
    writeInfoSection();
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

PageOffset LeafNodePage::getRIDSize(const KeyNumber &keyIndex, const AttrType &attrType) {
    if (keyIndex >= slotNumber) {
        throw std::invalid_argument("key index exceeds range.");
    }
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
    return ridsEndOffset - ridsStartOffset;
}

KeyNumber LeafNodePage::getRIDNumber(const KeyNumber &keyIndex, const AttrType &attrType) {
    if (keyIndex >= slotNumber) {
        throw std::invalid_argument("key index exceeds range.");
    }
    PageOffset ridSize = getRIDSize(keyIndex, attrType);
    assert(ridSize % sizeof(RID) == 0);
    return ridSize / sizeof(RID);
}

RID *LeafNodePage::getRID(const KeyNumber &keyIndex, const KeyNumber &ridIndex, const AttrType &attrType) {
    if (keyIndex >= slotNumber) {
        throw std::invalid_argument("key index exceeds range.");
    }
    void *key = getNthKey(keyIndex, attrType);
    PageOffset keyLength = getKeyLength(key, attrType);
    free(key);
    PageOffset ridsStartOffset = getNthKeyOffset(keyIndex) + keyLength;
    PageOffset ridsEndOffset;
    if (keyIndex == slotNumber - 1) {
        ridsEndOffset = getFreeSpaceOffset();
    } else {
        ridsEndOffset = getNthKeyOffset(keyIndex + 1);
    }
    PageOffset ridOffset = ridsStartOffset + sizeof(RID) * ridIndex;
    if (ridOffset >= ridsEndOffset) {
        throw std::invalid_argument("rid index exceeds range.");
    }
    void *rid = malloc(sizeof(RID));
    if (rid == nullptr) throw std::bad_alloc();
    memcpy(rid, (char *) pageData + ridOffset, sizeof(RID));
    return (RID *) rid;
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

    writeInfoSection();

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

    writeInfoSection();

    return 0;
}

RC LeafNodePage::getNextLeafPageID(PageNum &nextPageID) {
    nextPageID = nextLeafPage;
    if (nextLeafPage == NotExistPointer) return -1;
    return 0;
}

void LeafNodePage::setNextLeafPageID(const PageNum &nextPageID) {
    nextLeafPage = nextPageID;
    auto offset = PAGE_SIZE - infoSectionLength;
    memcpy((char *) pageData + offset, &nextPageID, sizeof(PageNum));
}

bool LeafNodePage::hasEnoughSpace(const Attribute &attribute, const void *key) {
    // add slot size;
    int sizeNeeded = sizeof(PageOffset);

    // add key size
    if (attribute.type == TypeVarChar) {
        PageOffset keyLength = getKeyLength(key, attribute.type);
        sizeNeeded += keyLength;
    } else if (attribute.type == TypeInt) {
        sizeNeeded += sizeof(int);
    } else if (attribute.type == TypeReal) {
        sizeNeeded += sizeof(float);
    } else {
        throw std::invalid_argument("invalid attribute type.");
    }

    // add rid size
    sizeNeeded += sizeof(RID);

    return sizeNeeded <= freeSpace;
}
