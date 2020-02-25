#include "ix.h"

IndexManager &IndexManager::instance() {
    static IndexManager _index_manager = IndexManager();
    return _index_manager;
}

RC IndexManager::createFile(const std::string &fileName) {
    return -1;
}

RC IndexManager::destroyFile(const std::string &fileName) {
    return -1;
}

RC IndexManager::openFile(const std::string &fileName, IXFileHandle &ixFileHandle) {
    return -1;
}

RC IndexManager::closeFile(IXFileHandle &ixFileHandle) {
    return -1;
}

RC IndexManager::insertEntry(IXFileHandle &ixFileHandle, const Attribute &attribute, const void *key, const RID &rid) {
    RC rc;
    PageNum root = ixFileHandle.getRootNodeID();
    PageNum returnedPointer = root;
    void* returnedKey = nullptr;
    rc = insertEntry(ixFileHandle, attribute, key, rid, root, returnedPointer, returnedKey);
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

    return -1;
}

void IndexManager::printBtree(IXFileHandle &ixFileHandle, const Attribute &attribute) const {
    PageNum root = ixFileHandle.getRootNodeID();
    printBTree(ixFileHandle, attribute, root, 0);
}

RC IndexManager::insertEntry(IXFileHandle &ixFileHandle, const Attribute &attribute, const void *key, const RID &rid,
                             PageNum cur, PageNum returnedPointer, void *returnedKey) {
    RC rc = 0;
    void* page = malloc(PAGE_SIZE);
    ixFileHandle.readNodePage(page, cur);
    bool isLeafNode = NodePage::isLeafPage(page);

    if (isLeafNode) { // leafNode
        LeafNodePage node(page, false);

        if (node.hasEnoughSpace(attribute, key)) {
            rc = node.addNewEntry(attribute, key, rid);
            ixFileHandle.writeNodePage(page, cur);
        } else {
            // split
            rc = splitLeafNode(ixFileHandle, attribute, cur, key, rid, returnedKey, returnedPointer);

            // if current page is root, create new root
            if (cur == ixFileHandle.getRootNodeId()) {
                PageNum newRootid;
                void* data = malloc(PAGE_SIZE);
                KeyNodePage newRoot(data);

                newRoot.addNewEntry(attribute, returnedKey, returnedPointer);
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
        // no split before;
        if (returnedKey == nullptr) return 0;
        // split detected, add returned entry
        if (node.hasEnoughSpace(attribute, returnedKey)) {
            node.addNewEntry(attribute, returnedKey, returnedPointer);
            free(returnedKey);
            returnedKey = nullptr;
            ixFileHandle.writeNodePage(page, cur);
        } else { // no enough space
            // split
            splitKeyNode(ixFileHandle, attribute, cur, returnedKey, returnedPointer);

            // if current page is root, create new root
            if (cur == ixFileHandle.getRootNodeId()) {
                PageNum newRootid;
                void* data = malloc(PAGE_SIZE);
                KeyNodePage newRoot(data);

                newRoot.addNewEntry(attribute, returnedKey, returnedPointer);
                newRoot.setLeftPointer(0, cur);
                ixFileHandle.writeNodePage(data, newRootid);
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
                             PageNum cur) {
    RC rc = 0;
    void* page = malloc(PAGE_SIZE);
    ixFileHandle.readNodePage(page, cur);
    bool isLeafNode = NodePage::isLeafPage(page);

    if (isLeafNode) { // reach leaf node, delete target rid
        LeafNodePage node(page, false);
        rc = node.deleteKey(key, attribute.type, rid);
        ixFileHandle.writeNodePage(page, cur);
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
    bool isLeaf = NodePage::isLeafPage(data);

    if (isLeaf) { // print leaf node in one line
        printSpace(level);
        std::cout << "{\"keys\": [";
        LeafNodePage node(data, false);
        SlotNumber slotNumber = node.getSlotNumber();
        void* tempKey;
        void* tempRID;

        PageOffset ridLength;
        // print each key and the following rids
        for (int i = 0; i < slotNumber; i++) {
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
            pageNum = node.getRightPointer(i);
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
    PageOffset ridLength = sizeof(PageNum) + sizeof(SlotNumber);
    PageNum pageNum;
    SlotNumber slotNum;
    while (length < totalLength) {
        memcpy(&pageNum, (char *) data + length, sizeof(PageNum));
        length += sizeof(PageNum);
        memcpy(&slotNum, (char *) data + length, sizeof(SlotNumber));
        length += sizeof(SlotNumber);
        std::cout << "(" << pageNum << "," << slotNum << ")";
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
            target = curNode.getRightPointer(i);
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
                                const void* middleKey, PageNum &newNodeId) {
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
    KeyNodePage newNode(newPage, movedBlock, keyNumber, dataLength, slotDataLength, id);

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
    newNode.setLeftPointer(0, curNode.getLeftPointer(splitPosition));

    // get middleKey
    void* middle;
    middle = curNode.getNthKey(splitPosition - 1, attribute.type);
    //delete half keys
    curNode.deleteKeysStartFrom(splitPosition - 1);

    // add new key
    if (curNode.compare(middleKey, middle, attribute.type) < 0) {
        curNode.addNewEntry(attribute, middleKey, newNodeId);
    } else {
        newNode.addNewEntry(attribute, middleKey, newNodeId);
    }

    // set middleKey
    if (attribute.type == TypeVarChar) {
        AttrLength length;
        memcpy(&length, (char *) middle, sizeof(AttrLength));
        memcpy((char *) middleKey, (char *) middle, sizeof(AttrLength) + length);
    } else {
        memcpy((char *) middleKey, (char *) middle, sizeof(int));
    }

    ixFileHandle.writeNodePage(curPage, id);
    ixFileHandle.appendNodePage(newPage, newNodeId);

    free(curPage);
    free(newPage);
    free(middle);
    free(movedBlock);
}

RC IndexManager::splitLeafNode(IXFileHandle &ixFileHandle, const Attribute &attribute, const PageNum id, const void* key,
                                 const RID &rid, const void *middleKey, PageNum &newNodeId) {
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
    LeafNodePage newNode(newPage, movedBlock, keyNumber, dataLength, slotDataLength, id);

    // set middleKey
    void* middle = curNode.getNthKey(splitPosition - 1, attribute.type);
    if (attribute.type == TypeVarChar) {
        AttrLength length;
        memcpy(&length, (char *) middle, sizeof(AttrLength));
        memcpy((char *) middleKey, (char *) middle, sizeof(AttrLength) + length);
    } else {
        memcpy((char *) middleKey, (char *) middle, sizeof(int));
    }
    free(middle);

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

    // set next Pointer
    PageNum nextLeafPointer;
    curNode.getNextLeafPageID(nextLeafPointer);
    curNode.setNextLeafPageID(newNodeId);
    newNode.setNextLeafPageID(nextLeafPointer);

    // delete half entries
    curNode.deleteKeysStartFrom(splitPosition - 1);

    // add new key
    if (curNode.compare(key, middleKey, attribute.type) < 0) {
        rc = curNode.addNewEntry(attribute, key, rid);
    } else {
        rc = newNode.addNewEntry(attribute, key, rid);
    }

    ixFileHandle.writeNodePage(curPage, id);
    ixFileHandle.appendNodePage(newPage, newNodeId);
    free(curPage);
    free(newPage);
    free(movedBlock);
    return rc;
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
}

IXFileHandle::~IXFileHandle() {
}

RC IXFileHandle::collectCounterValues(unsigned &readPageCount, unsigned &writePageCount, unsigned &appendPageCount) {
    return -1;
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


bool LeafNodePage::hasEnoughSpace(const Attribute &attribute, const void *key) {
    // add slot size;
    int sizeNeeded = sizeof(PageOffset);

    // add key size
    if (attribute.type == TypeVarChar) {
        int length;
        memcpy(&length, (char *) key, sizeof(int));
        sizeNeeded += length + sizeof(int);
    } else {
        sizeNeeded += sizeof(int);
    }

    // add rid size
    sizeNeeded += sizeof(RID);

    return sizeNeeded <= freeSpace;
}

RC LeafNodePage::addNewEntry(const Attribute &attribute, const void *key, const RID &rid) {
    RC rc;
    rc = addKey(key, attribute.type, rid);
    return rc;
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

void KeyNodePage::addNewEntry(const Attribute &attribute, const void *key, const PageNum &returnedPointer) {
    PageNum newKeyIndex;
    addKey(key, attribute.type, newKeyIndex);
    setRightPointer(newKeyIndex, returnedPointer);
}
