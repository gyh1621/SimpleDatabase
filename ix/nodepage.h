#include "../types.h"

#ifndef CS222P_WINTER20_NODEPAGE_H
#define CS222P_WINTER20_NODEPAGE_H

class NodePage {

    /*
    * Basic page format (shared structure between KeyNode and LeafNode):
     *
     * offset in slots points to the start of the key
     * slots grows from page's end to start
     *
    * ┌────────────────────────────────────────────────────────────────────────────────┐
    * │ DATA SECTION                                                                   │
    * │                                                                                │
    * │    ┌───────────────────┬────────────────┬─────────────────┬────────────────────┤
    * │    │   SLOT DIRECTORY  │   FREE SPACE   │   SLOT NUMBER   │       IS LEAF      │
    * │    ├───────────────────┼────────────────┼─────────────────┼────────────────────┤
    * │    │      <slots>      │   free space   │   slot number   │  node type (bool)  │
    * └────┴───────────────────┴────────────────┴─────────────────┴────────────────────┘
    */

public:
    // pages are supposed not reach max number
    static const PageNum NotExistPointer = std::numeric_limits<PageNum>::max();

protected:

    PageFreeSpace freeSpace;
    SlotNumber slotNumber;
    bool isLeaf;
    void *pageData;
    PageOffset infoSectionLength = sizeof(bool) + sizeof(SlotNumber) + sizeof(PageFreeSpace);

    // passed page data, will not be delete in destructor
    // init empty data
    NodePage(void *pageData, bool init);
    NodePage(const NodePage&) = delete;                             // copy constructor, implement when needed
    NodePage(NodePage&&) = delete;                                  // move constructor, implement when needed
    NodePage& operator=(const NodePage&) = delete;                  // copy assignment, implement when needed
    NodePage& operator=(NodePage&&) = delete;                       // move assignment, implement when needed
    ~NodePage() = default;

    /* write info section */
    void writeInfoSection();

    /* read info section */
    void readInfoSection();

    /* get current free space start offset */
    PageOffset getFreeSpaceOffset();

    void moveData(const PageOffset &startOffset, const PageOffset &targetOffset, const PageOffset &length);

    /* update slots */
    // if add is true, add "deviateOffset" from startSlot to endSlot(included)
    void updateSlots(const SlotNumber &startSlot, const SlotNumber &endSlot, const PageOffset &deviateOffset, bool add);

    PageOffset getNthSlotOffset(const KeyNumber &keyIndex);
    PageOffset getNthKeyOffset(const KeyNumber &keyIndex);

    /* write nth key's offset to the corresponding slot */
    // this slot should exist, otherwise will throw error
    void writeNthSlot(const KeyNumber &keyIndex, const PageOffset &keyOffset);

    /* find position of key */
    // "key" points to "length + string" if type is varchar
    // if exits, return true, key index is actual position
    // if not exits, return false, key index is key's position if inserted
    bool findKey(const void *key, const AttrType &attrType, SlotNumber &keyIndex);

public:

    /* is *data a leaf node page data or key node page data */
    static bool isLeafNode(void *data);

    /* compute a key's length */
    static PageOffset getKeyLength(const void *key, const AttrType &attrType);

    PageFreeSpace getFreeSpace() { return freeSpace; };
    SlotNumber getSlotNumber() { return slotNumber; };
    void *getPageData() { return pageData; }

    /* compare two data based on type */
    // when type is varchar, data points to "length + string"
    // if data1 > data2, return positive;
    // if data2 < data2, return negative;
    // if data1 == data2, return 0;
    static RC compare(const void *data1, const void *data2, const AttrType &attrType);

    /* Delete keys start from keyIndex */
    void deleteKeysStartFrom(const KeyNumber &keyIndex);

    /* Get nth key data, n starts from 0 */
    // if type is varchar, will return a pointer to "length + string"
    // if key type is int/float, return a int/float pointer
    // error, return nullptr
    void* getNthKey(const KeyNumber &keyIndex, const AttrType &attrType);

    /* Copy data and slots */
    // suppose one page is:
    //  k0, k1, k2,.................
    //  ............kn,.............
    //  ......s0, s1, s2..., sn,....
    // after executing:
    //   "void* block = copyToEnd(1, dataLength, slotDataLength);"
    // block will points to a memory block: k1, k2, ....., kn, s1, s2, ...., sn
    //                                     |    dataLength + slotDataLength  |
    // dataLength = address of kn - address of k1
    // slotDataLength = address if sn - address of s1
    // keyNumbers = n - 1 + 1 = n
    //
    // for key node page: kn is actually kn and pointer after kn
    // for leaf node page: kn is actually kn and rids of kn
    //
    // if fail, return nullptr
    void* copyToEnd(const KeyNumber &startKey, PageOffset &dataLength, PageOffset &slotDataLength, KeyNumber &keyNumbers);

};


class KeyNodePage: public NodePage {

    /* page format: same as NodePage */

public:
    // passed page data, will not be delete in destructor
    explicit KeyNodePage(void *pageData, bool init=true);

    // used in spliting, *block comes from "copyToEnd"
    KeyNodePage(void *pageData, const void* block, const KeyNumber &keyNumbers,
                const PageOffset &dataLength, const PageOffset &slotDataLength);

    KeyNodePage(const KeyNodePage&) = delete;                             // copy constructor, implement when needed
    KeyNodePage(KeyNodePage&&) = delete;                                  // move constructor, implement when needed
    KeyNodePage& operator=(const KeyNodePage&) = delete;                  // copy assignment, implement when needed
    KeyNodePage& operator=(KeyNodePage&&) = delete;                       // move assignment, implement when needed
    ~KeyNodePage() = default;

    KeyNumber getKeyNumber() { return getSlotNumber(); };

    /* NOTE: All methods below will throw assert error when key indexes are not valid */

    /* Add a key */
    // not check whether has enough space
    // not assign pointers of the key, only add a key to the page
    // when key is varchar, "key" is a pointer to "length" + "string"
    //
    // if page's content is like below and now k is to be inserted, k < k0:
    // p0, k0, p1, k1, p2, k2, ...
    //    ↑ k will inserted here with a new pointer p
    // return 0 - success, other- fail
    void addKey(const void *key, const AttrType &attrType, KeyNumber &keyIndex);

    /* Set pointers of a key */
    void setLeftPointer(const KeyNumber &keyIndex, const PageNum &pageID);
    void setRightPointer(const KeyNumber &keyIndex, const PageNum &pageID, const AttrType &attrType);

    /* Get pointers of a key */
    PageNum getLeftPointer(const KeyNumber &keyIndex);
    PageNum getRightPointer(const KeyNumber &keyIndex, const AttrType &attrType);

    // check if current page has enough space for the new key
    bool hasEnoughSpace(const Attribute &attribute, const void *key);
};


class LeafNodePage: public NodePage {

    /*
    * Leaf page format:
    * ┌────────────────────────────────────────────────────────────────────────────────────────────────────┐
    * │ <key, [rid, rid, rid]>, ...                                                                        │
    * │                                                                                                    │
    * │    ┌───────────────────┬───────────────────┬────────────────┬─────────────────┬────────────────────┤
    * │    │   SLOT DIRECTORY  │    NEXT POINTER   │   FREE SPACE   │   SLOT NUMBER   │       IS LEAF      │
    * │    ├───────────────────┼───────────────────┼────────────────┼─────────────────┼────────────────────┤
    * │    │      <slots>      │ next leaf page id │   free space   │   slot number   │  node type (bool)  │
    * └────┴───────────────────┴───────────────────┴────────────────┴─────────────────┴────────────────────┘
    */

    // default is 0 indicating no next leaf page
    PageNum nextLeafPage;

private:

    /* find whether rid exists */
    // if not exist, return 0, else return rid start offset
    PageOffset findRid(const KeyNumber &keyIndex, const AttrType &attrType, const RID &rid);

    void readInfoSection();

    void writeInfoSection();

public:
    // passed page data, will not be delete in destructor
    LeafNodePage(void *pageData, bool init=true);

    // used in spliting, *block comes from "copyToEnd"
    LeafNodePage(void *pageData, const void* block, const KeyNumber &keyNumbers,
                 const PageOffset &dataLength, const PageOffset &slotDataLength);

    LeafNodePage(const LeafNodePage&) = delete;                        // copy constructor, implement when needed
    LeafNodePage(LeafNodePage&&) = delete;                             // move constructor, implement when needed
    LeafNodePage& operator=(const LeafNodePage&) = delete;             // copy assignment, implement when needed
    LeafNodePage& operator=(LeafNodePage&&) = delete;                  // move assignment, implement when needed
    ~LeafNodePage() = default;

    /* Add a RID */
    // not check whether has enough space
    // when key is varchar, "key" is a pointer to "length" + "string"
    // if key already exists, the new rid will append to the existed rids of the same key
    // return 0 - success, 1 - rid already exists (with same key)
    RC addKey(const void *key, const AttrType &attrType, const RID &rid);

    /* Get size of rids of a key */
    PageOffset getRIDSize(const KeyNumber &keyIndex, const AttrType &attrType);

    /* Get number of rid of a key */
    KeyNumber getRIDNumber(const KeyNumber &keyIndex, const AttrType &attrType);

    /* Get a rid of a key */
    RID *getRID(const KeyNumber &keyIndex, const KeyNumber &ridIndex, const AttrType &attrType);

    /* Get rids of a key */
    void *getRIDs(const KeyNumber &keyIndex, PageOffset &dataLength, const AttrType &attrType);

    /* Delete a RID */
    // when key is varchar, "key" is a pointer to "length" + "string"
    // return 0 - success, 1 - rid/key not exist
    RC deleteKey(const void *key, const AttrType &attrType, const RID &rid);

    /* Get next leaf page's pointer */
    // return 0 - success, other - no next leaf page
    RC getNextLeafPageID(PageNum &nextPageID);

    /* Set next leaf page's pointer */
    void setNextLeafPageID(const PageNum &nextPageID);

    bool hasEnoughSpace(const Attribute &attribute, const void *key);
};

#endif //CS222P_WINTER20_NODEPAGE_H
