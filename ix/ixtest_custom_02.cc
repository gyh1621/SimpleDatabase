#include <cstring>
#define private public
#define protected public
#include "ix.h"
#include "ix_test_util.h"

/* Test functions of NodePage */

// Functions left to test in KeyNodePage and LeafNodePage:
//      getFreeSpaceOffset()
//      findKey()
//      isLeafNode()
//      deleteKeysFrom()
//      getNthKey()
//      copyToEnd()

// Functions not test:
//      getFreeSpace()
//      getSlotNumber()
//      getPageData()


void testNodePageConstructor() {
    void *data = malloc(PAGE_SIZE);
    NodePage nodePage1(data, true);
    assert(nodePage1.freeSpace == PAGE_SIZE - sizeof(bool) - sizeof(SlotNumber) - sizeof(PageFreeSpace));
    NodePage nodePage2(data, false);
    assert(nodePage2.freeSpace == nodePage1.freeSpace
           && nodePage2.slotNumber == nodePage1.slotNumber
           && nodePage2.isLeaf == nodePage1.isLeaf);
    free(data);
}

void testNodePageMoveData() {
    void *pageData = malloc(PAGE_SIZE);
    NodePage nodePage(pageData, true);
    int dataLength = sizeof(int) * 500;
    void *block = malloc(dataLength);
    for (int i = 0; i < 500; i++) {
        memcpy((char *) block + i * sizeof(int), &i, sizeof(int));
    }
    memcpy(pageData, block, dataLength);
    nodePage.moveData(0, 300, dataLength);
    void *tmp = malloc(dataLength);
    memcpy(tmp, (char *) pageData + 300, dataLength);
    assert(memcmp(tmp, block, dataLength) == 0);
    nodePage.moveData(300, 0, dataLength);
    memcpy(tmp, pageData, dataLength);
    assert(memcmp(tmp, block, dataLength) == 0);
    free(pageData);
    free(block);
    free(tmp);
}

void testNodePageUpdateSlots() {
    void *pageData = malloc(PAGE_SIZE);
    NodePage nodePage(pageData, true);
    nodePage.slotNumber = 100;
    PageOffset offset = PAGE_SIZE - nodePage.infoSectionLength - sizeof(PageOffset);
    // write slots
    for (SlotNumber i = 0; i < nodePage.slotNumber; i++) {
        PageOffset keyOffset = i * i;
        memcpy((char *) pageData + offset, &keyOffset, sizeof(PageOffset));
        offset -= sizeof(PageOffset);
    }
    // update slots, add
    nodePage.updateSlots(0, nodePage.slotNumber - 1, 1, true);
    // read slots
    offset = PAGE_SIZE - nodePage.infoSectionLength - sizeof(PageOffset);
    for (SlotNumber i = 0; i < nodePage.slotNumber; i++) {
        PageOffset keyOffset;
        memcpy(&keyOffset, (char *) pageData + offset, sizeof(PageOffset));
        assert(keyOffset == i * i + 1);
        offset -= sizeof(PageOffset);
    }
    // update slots, subtract
    nodePage.updateSlots(0, nodePage.slotNumber - 1, 1, false);
    // read slots
    offset = PAGE_SIZE - nodePage.infoSectionLength - sizeof(PageOffset);
    for (SlotNumber i = 0; i < nodePage.slotNumber; i++) {
        PageOffset keyOffset;
        memcpy(&keyOffset, (char *) pageData + offset, sizeof(PageOffset));
        assert(keyOffset == i * i);
        offset -= sizeof(PageOffset);
    }
    free(pageData);
}

void testNodePageGetNthSlotOffset() {
    void *pageData = malloc(PAGE_SIZE);
    NodePage nodePage(pageData, true);
    nodePage.slotNumber = 100;
    PageOffset offset = PAGE_SIZE - nodePage.infoSectionLength - sizeof(PageOffset);
    for (SlotNumber i = 0; i < nodePage.slotNumber; i++) {
        PageOffset slotOffset = nodePage.getNthSlotOffset(i);
        assert(slotOffset == offset);
        offset -= sizeof(PageOffset);
    }
    free(pageData);
}

void testNodePageGetNthKeyOffset() {
    void *pageData = malloc(PAGE_SIZE);
    NodePage nodePage(pageData, true);
    nodePage.slotNumber = 100;
    PageOffset offset = PAGE_SIZE - nodePage.infoSectionLength - sizeof(PageOffset);
    // write slots
    for (SlotNumber i = 0; i < nodePage.slotNumber; i++) {
        PageOffset keyOffset = i * i;
        memcpy((char *) pageData + offset, &keyOffset, sizeof(PageOffset));
        offset -= sizeof(PageOffset);
    }
    // get key offset
    for (KeyNumber i = 0; i < nodePage.slotNumber; i++) {
        PageOffset keyOffset1 = i * i;
        PageOffset keyOffset2 = nodePage.getNthKeyOffset(i);
        assert(keyOffset1 == keyOffset2);
    }
    free(pageData);
}

void testNodePageWriteNthSlot() {
    void *pageData = malloc(PAGE_SIZE);
    NodePage nodePage(pageData, true);
    nodePage.slotNumber = 100;
    // write slots
    for (SlotNumber i = 0; i < nodePage.slotNumber; i++) {
        nodePage.writeNthSlot(i, i * i + i);
    }
    // read slots
    PageOffset offset = PAGE_SIZE - nodePage.infoSectionLength - sizeof(PageOffset);
    for (SlotNumber i = 0; i < nodePage.slotNumber; i++) {
        PageOffset keyOffset;
        memcpy(&keyOffset, (char *) pageData + offset, sizeof(PageOffset));
        assert(keyOffset == i * i + i);
        offset -= sizeof(PageOffset);
    }
    free(pageData);
}

void testNodePageGetKeyLength() {
    void *key = malloc(8);
    PageOffset size = 5;
    memcpy(key, &size, 4);
    assert(NodePage::getKeyLength(key, TypeInt) == sizeof(int));
    assert(NodePage::getKeyLength(key, TypeReal) == sizeof(float));
    assert(NodePage::getKeyLength(key, TypeVarChar) == size + 4);
    free(key);
}

void testNodePageCompare() {
    void *d1 = malloc(100), *d2 = malloc(100);
    float f1 = 0.5, f2 = 0.3;
    memcpy(d1, &f1, sizeof(float));
    memcpy(d2, &f2, sizeof(float));
    assert(NodePage::compare(d1, d2, TypeReal) > 0);
    int i1 = 1, i2 = 2;
    memcpy(d1, &i1, sizeof(int));
    memcpy(d2, &i2, sizeof(int));
    assert(NodePage::compare(d1, d2, TypeInt) < 0);
    memset(d1, 0, 100);
    memset(d2, 0, 100);
    int l1 = 20, l2 = 20;
    memcpy(d1, &l1, 4);
    memcpy(d2, &l2, 4);
    char c;
    for (int i = 4; i < 24; i++) {
        c = 'a' + i;
        memcpy((char *) d1 + i, &c, 1);
    }
    for (int i = 4; i < 23; i++) {
        c = 'a' + i;
        memcpy((char *) d2 + i, &c, 1);
    }
    c = 'a' + 24;
    memcpy((char *) d2 + 23, &c, 1);
    assert(NodePage::compare(d1, d2, TypeVarChar) < 0);
    l2--;
    memcpy(d2, &l2, 4);
    assert(NodePage::compare(d1, d2, TypeVarChar) > 0);
    l2++;
    memcpy(d2, &l2, 4);
    c = 'a' + 23;
    memcpy((char *) d2 + 23, &c, 1);
    assert(NodePage::compare(d1, d2, TypeVarChar) == 0);
    free(d1);
    free(d2);
}


int main() {

    std::cout << std::endl << "***** In IX Custom Test Case 02 *****" << std::endl;

    testNodePageConstructor();
    testNodePageGetNthSlotOffset();
    testNodePageGetNthKeyOffset();
    testNodePageMoveData();
    testNodePageUpdateSlots();
    testNodePageWriteNthSlot();
    testNodePageGetKeyLength();
    testNodePageCompare();

    std::cout << std::endl << "***** IX Custom Test Case 02 Finished *****" << std::endl;

    return success;
}
