#include <cstring>
#define private public
#define protected public
#include "ix.h"
#include "ix_test_util.h"
#include <numeric>

/* Test functions of KeyNodePage */


void testKeyNodePageConstructor() {
    void *pageData = malloc(PAGE_SIZE);
    assert(!NodePage::isLeafNode(pageData));
    KeyNodePage nodePage1(pageData, true);
    bool isLeaf;
    memcpy(&isLeaf, (char *) pageData + PAGE_SIZE - 1, 1);
    assert(!isLeaf);
    KeyNodePage nodePage2(pageData, false);
    memcpy(&isLeaf, (char *) pageData + PAGE_SIZE - 1, 1);
    assert(!isLeaf);
    free(pageData);
}

void testKeyNodePage() {
    void *pageData = malloc(PAGE_SIZE);
    KeyNodePage nodePage(pageData, true);
    PageFreeSpace startFS = nodePage.freeSpace;

    void *key = malloc(PAGE_SIZE);

    // add varchar keys
    int keyNumbers = 50;
    int maxVarcharLength = 20;
    KeyNumber keyIndex;
    int inserted = 0;
    int keyLengths[keyNumbers];
    for (int i = keyNumbers / 2; i < keyNumbers; i++) {
        int varcharLength = (i + 1) % maxVarcharLength;
        if (varcharLength == 0) varcharLength++;
        buildVarcharKey(i, varcharLength, key);
        keyLengths[i] = varcharLength + 4;
        nodePage.addKey(key, TypeVarChar, keyIndex);
        inserted++;
        std::cout << "inserted " << inserted << " key index: " << keyIndex << std::endl;
    }
    for (int i = 0; i < keyNumbers / 2; i++) {
        int varcharLength = (i + 1) % maxVarcharLength;
        if (varcharLength == 0) varcharLength++;
        buildVarcharKey(i, varcharLength, key);
        keyLengths[i] = varcharLength + 4;
        nodePage.addKey(key, TypeVarChar, keyIndex);
        inserted++;
        std::cout << "inserted " << inserted << " key index: " << keyIndex << std::endl;
    }

    // check key numbers
    assert(nodePage.getKeyNumber() == keyNumbers);

    // check free space
    int occupiedSize = std::accumulate(keyLengths, keyLengths + keyNumbers, 0)  // keys' size
                       + sizeof(PageNum) * (keyNumbers + 1)  // pointers' size
                       + sizeof(PageOffset) * keyNumbers;  // slots' size
    std::cout << "freespace at start: " << startFS << " freespace now: " << nodePage.freeSpace << " occupied size: " << occupiedSize << std::endl;
    assert(startFS - nodePage.freeSpace == occupiedSize);
    assert(nodePage.getFreeSpaceOffset()
           == std::accumulate(keyLengths, keyLengths + keyNumbers, 0)
              + sizeof(PageNum) * (keyNumbers + 1));

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

    // test deleteKeysStartFrom
    PageFreeSpace beforeDeleteFS = nodePage.freeSpace;
    nodePage.deleteKeysStartFrom(keyNumbers / 2);
    KeyNumber deletedKeys = keyNumbers - keyNumbers / 2;
    PageOffset deletedLength = std::accumulate(keyLengths + keyNumbers / 2, keyLengths + keyNumbers, 0)
                               + sizeof(PageNum) * deletedKeys
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
    KeyNodePage nodePage1(pageData1, block, keyNumber, dataLength, slotDataLength);
    PageFreeSpace supposedFS = PAGE_SIZE
                               - nodePage1.infoSectionLength
                               - std::accumulate(keyLengths, keyLengths + keyNumbers / 2, 0)
                               - sizeof(PageOffset) * keyNumber
                               - sizeof(PageNum) * (keyNumber + 1);
    std::cout << dataLength << " " << slotDataLength << " " << keyNumber << std::endl;
    std::cout << std::accumulate(keyLengths, keyLengths + keyNumbers / 2, 0) << " " << sizeof(PageNum) * (keyNumber + 1)
              << " " << sizeof(PageOffset) * keyNumber << std::endl;
    std::cout << "supposed freespace: " << supposedFS << " actual freespace: " << nodePage1.freeSpace << std::endl;
    assert(supposedFS == nodePage1.freeSpace);
    assert(keyNumber == nodePage1.slotNumber);
    assert(!nodePage1.isLeaf);

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
    free(block);
    free(pageData);
    free(pageData1);
}



int main() {

    std::cout << std::endl << "***** In IX Custom Test Case 03 *****" << std::endl;

    testKeyNodePageConstructor();
    testKeyNodePage();

    std::cout << std::endl << "***** IX Custom Test Case 03 Finished *****" << std::endl;

    return success;
}
