#include <cstring>
#define private public
#define protected public
#include "ix.h"
#include "ix_test_util.h"
#include <numeric>

/* Test functions of LeafNodePage */

// Functions left to test in KeyNodePage and LeafNodePage:
//      getFreeSpaceOffset()
//      findKey()
//      isLeafNode()
//      deleteKeysFrom()
//      getNthKey()
//      copyToEnd()


void testLeafNodePageConstructor() {
    void *pageData = malloc(PAGE_SIZE);
    assert(!NodePage::isLeafNode(pageData));
    LeafNodePage nodePage1(pageData, true);
    bool isLeaf;
    memcpy(&isLeaf, (char *) pageData + PAGE_SIZE - 1, 1);
    assert(isLeaf);
    LeafNodePage nodePage2(pageData, false);
    memcpy(&isLeaf, (char *) pageData + PAGE_SIZE - 1, 1);
    assert(isLeaf);
    free(pageData);
}

void testLeafNodePage() {
    void *pageData = malloc(PAGE_SIZE);
    LeafNodePage nodePage(pageData, true);
    PageFreeSpace startFS = nodePage.freeSpace;

    // add varchar keys
    void *key = malloc(PAGE_SIZE);
    int keyNumbers = 21;
    int maxVarcharLength = 20;
    KeyNumber keyIndex;
    int inserted = 0;
    int keyLengths[keyNumbers];
    for (int i = keyNumbers / 2; i < keyNumbers; i++) {
        int varcharLength = (i + 1) % maxVarcharLength;
        if (varcharLength == 0) varcharLength++;
        buildVarcharKey(i, varcharLength, key);
        for (int j = 0; j < i + 1; j++) {
            RID rid;
            rid.pageNum = rid.slotNum = i + j;
            nodePage.addKey(key, TypeVarChar, rid);
        }
        inserted++;
        std::cout << "inserted " << inserted << " key index: " << keyIndex << std::endl;
        keyLengths[i] = varcharLength + 4 + sizeof(RID) * (i + 1);
    }
    for (int i = 0; i < keyNumbers / 2; i++) {
        RID rid;
        rid.pageNum = rid.slotNum = i;
        int varcharLength = (i + 1) % maxVarcharLength;
        if (varcharLength == 0) varcharLength++;
        buildVarcharKey(i, varcharLength, key);
        for (int j = 0; j < i + 1; j++) {
            RID rid;
            rid.pageNum = rid.slotNum = i + j;
            nodePage.addKey(key, TypeVarChar, rid);
        }
        nodePage.addKey(key, TypeVarChar, rid);
        inserted++;
        std::cout << "inserted " << inserted << " key index: " << keyIndex << std::endl;
        keyLengths[i] = varcharLength + 4 + sizeof(RID) * (i + 1);
    }

    // check key numbers
    assert(nodePage.slotNumber == keyNumbers);

    // check free space
    int occupiedSize = std::accumulate(keyLengths, keyLengths + keyNumbers, 0)  // keys' size
                       + sizeof(PageOffset) * keyNumbers;  // slots' size
    std::cout << "freespace at start: " << startFS << " freespace now: " << nodePage.freeSpace << " occupied size: " << occupiedSize << std::endl;
    assert(startFS - nodePage.freeSpace == occupiedSize);
    assert(nodePage.getFreeSpaceOffset() == std::accumulate(keyLengths, keyLengths + keyNumbers, 0));

    // check getNthKey and findKey
    for (int i = 0; i < keyNumbers; i++) {
        int varcharLength = (i + 1) % maxVarcharLength;
        if (varcharLength == 0) varcharLength++;
        buildVarcharKey(i, varcharLength, key);
        // NOTE: key returned by "buildVarcharKey" becomes "greater" with seed become bigger,
        //       so after inserting keys, keyIndex is same to i
        void *key2 = nodePage.getNthKey(i, TypeVarChar);
        assert(NodePage::compare(key, key2, TypeVarChar) == 0);
        free(key2);
        nodePage.findKey(key, TypeVarChar, keyIndex);
        assert(keyIndex == i);
    }

    // check getRIDs
    for (KeyNumber i = 0; i < keyNumbers; i++) {
        void *supposedRIDs = malloc(sizeof(RID) * (i + 1));
        for (int j = 0; j < i + 1; j++) {
            RID rid;
            rid.pageNum = rid.slotNum = i + j;
            memcpy((char *) supposedRIDs + j * sizeof(RID), &rid, sizeof(RID));
        }
        PageOffset ridsLength;
        void *rids = nodePage.getRIDs(i, ridsLength, TypeVarChar);
        assert(ridsLength == sizeof(RID) * (i + 1));
        assert(memcmp(rids, supposedRIDs, ridsLength) == 0);
        free(rids);
        free(supposedRIDs);
    }

    // check delete
    // delete a key's all rids if keyIndex % 5 == 0
    // else delete the middle rid
    PageOffset deletedLength = 0;
    KeyNumber deletedKey = 0;
    PageFreeSpace beforeDelFS = nodePage.freeSpace;
    for (KeyNumber i = 0; i < keyNumbers; i++) {
        int varcharLength = (i + 1) % maxVarcharLength;
        if (varcharLength == 0) varcharLength++;
        buildVarcharKey(i, varcharLength, key);
        assert(nodePage.findKey(key, TypeVarChar, keyIndex));
        if (i != keyNumbers - 1) {
            std::cout << "start deleting key:" << i << " key offset:" << nodePage.getNthKeyOffset(keyIndex)
                      << " next key offset:" << nodePage.getNthKeyOffset(keyIndex + 1) << std::endl;
        }
        if (i % 5 == 0) {
            for (int j = 0; j < i + 1; j++) {
                RID rid;
                rid.pageNum = rid.slotNum = i + j;
                RC rc = nodePage.deleteKey(key, TypeVarChar, rid);
                assert(rc == 0);
                std::cout << "delete one rid " << j << " of key: " << i << " delete length:" << sizeof(RID) << std::endl;
            }
            deletedLength += keyLengths[i] + sizeof(PageOffset);
            std::cout << "delete key: " << i << " delete length:" << keyLengths[i] + sizeof(PageOffset) << std::endl;
            keyLengths[i] = 0;
            deletedKey++;
        } else {
            RID rid;
            rid.pageNum = rid.slotNum = i + (i + 1) / 2;
            RC rc = nodePage.deleteKey(key, TypeVarChar, rid);
            assert(rc == 0);
            std::cout << "delete one rid of key: " << i << " delete length:" << sizeof(RID) << std::endl;
            deletedLength += sizeof(RID);
            keyLengths[i] -= sizeof(RID);
        }
        std::cout << "before fs:" << beforeDelFS << " deleted length:" << deletedLength << " fs now:" << nodePage.freeSpace << std::endl << std::endl;
        assert(beforeDelFS + deletedLength == nodePage.freeSpace);
    }
    assert(keyNumbers - deletedKey == nodePage.slotNumber);

    // check getRIDs after deleting
    int needDeleteIndex = 0;
    for (KeyNumber i = 0; i < keyNumbers; i++) {
        if (i % 5 == 0) {
            needDeleteIndex++;
            continue;
        }
        keyIndex = i - needDeleteIndex;
        void *supposedRIDs = malloc(sizeof(RID) * i);
        int ridNum = 0;
        for (int j = 0; j < i + 1; j++) {
            if (j == (i + 1) / 2) continue;
            RID rid;
            rid.pageNum = rid.slotNum = i + j;
            memcpy((char *) supposedRIDs + ridNum * sizeof(RID), &rid, sizeof(RID));
            ridNum++;
        }
        PageOffset ridsLength;
        void *rids = nodePage.getRIDs(keyIndex, ridsLength, TypeVarChar);
        std::cout << ridsLength << " " << i << std::endl;
        assert(ridsLength == sizeof(RID) * i);
        assert(memcmp(rids, supposedRIDs, ridsLength) == 0);
        free(rids);
        free(supposedRIDs);
    }

    free(key);
    free(pageData);
}

void testLeadNodePage2() {
    void *pageData = malloc(PAGE_SIZE);
    LeafNodePage nodePage(pageData, true);
    PageFreeSpace startFS = nodePage.freeSpace;

    // add varchar keys
    void *key = malloc(PAGE_SIZE);
    int keyNumbers = 21;
    int maxVarcharLength = 20;
    KeyNumber keyIndex;
    int inserted = 0;
    int keyLengths[keyNumbers];
    for (int i = 0; i < keyNumbers; i++) {
        int varcharLength = (i + 1) % maxVarcharLength;
        if (varcharLength == 0) varcharLength++;
        buildVarcharKey(i, varcharLength, key);
        for (int j = 0; j < i + 1; j++) {
            RID rid;
            rid.pageNum = rid.slotNum = i + j;
            nodePage.addKey(key, TypeVarChar, rid);
        }
        inserted++;
        std::cout << "inserted " << inserted << " key index: " << keyIndex << std::endl;
        keyLengths[i] = varcharLength + 4 + sizeof(RID) * (i + 1);
    }

    // test deleteKeysStartFrom
    PageFreeSpace beforeDeleteFS = nodePage.freeSpace;
    nodePage.deleteKeysStartFrom(keyNumbers / 2);
    KeyNumber deletedKeys = keyNumbers - keyNumbers / 2;
    PageOffset deletedLength = std::accumulate(keyLengths + keyNumbers / 2, keyLengths + keyNumbers, 0)
                               + sizeof(PageOffset) * deletedKeys;
    std::cout << "freespace before delete: " << beforeDeleteFS
              << " freespace after delete: " << nodePage.freeSpace
              << " computed deleted freespace: " << deletedLength << std::endl;
    assert(nodePage.freeSpace - beforeDeleteFS == deletedLength);
    assert(nodePage.slotNumber = keyNumbers - deletedKeys);

    // test copyToEnd
    PageOffset dataLength, slotDataLength;
    KeyNumber keyNumber;
    void *block = nodePage.copyToEnd(0, dataLength, slotDataLength, keyNumber);
    void *pageData1 = malloc(PAGE_SIZE);
    LeafNodePage nodePage1(pageData1, block, keyNumber, dataLength, slotDataLength);
    PageFreeSpace supposedFS = PAGE_SIZE
                               - nodePage1.infoSectionLength
                               - std::accumulate(keyLengths, keyLengths + keyNumbers / 2, 0)
                               - sizeof(PageOffset) * keyNumber;
    std::cout << dataLength << " " << slotDataLength << " " << keyNumber << std::endl;
    std::cout << "supposed freespace: " << supposedFS << " actual freespace: " << nodePage1.freeSpace << std::endl;
    assert(supposedFS == nodePage1.freeSpace);
    assert(keyNumber == nodePage1.slotNumber);
    assert(nodePage1.isLeaf);

    // test keys after copying
    for (KeyNumber i = 0; i < keyNumber; i++) {
        int varcharLength = (i + 1) % maxVarcharLength;
        if (varcharLength == 0) varcharLength++;
        buildVarcharKey(i, varcharLength, key);
        void *key2 = nodePage1.getNthKey(i, TypeVarChar);
        assert(NodePage::compare(key, key2, TypeVarChar) == 0);
        free(key2);
        nodePage1.findKey(key, TypeVarChar, keyIndex);
        assert(keyIndex == i);
    }

    free(key);
    free(pageData);
    free(pageData1);
}


int main() {

    std::cout << std::endl << "***** In IX Custom Test Case 04 *****" << std::endl;

    testLeafNodePageConstructor();
    testLeafNodePage();
    testLeadNodePage2();

    std::cout << std::endl << "***** IX Custom Test Case 04 Finished *****" << std::endl;

    return success;
}
