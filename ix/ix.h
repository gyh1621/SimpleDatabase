#ifndef _ix_h_
#define _ix_h_

#include <vector>
#include <string>
#include <fstream>

#include "../rbf/rbfm.h"
#include "nodepage.h"

# define IX_EOF (-1)  // end of the index scan

class IX_ScanIterator;

class IXFileHandle;

class IndexManager {

private:
    bool fileExist(const std::string &fileName);

    // insert new Entry
    // if returnedKey key is not null, it means split happened before,
    // a new key and its right  pointer need to be added
    // 0 - success, other - rid exists
    RC insertEntry(IXFileHandle &ixFileHandle, const Attribute &attribute, const void *key, const RID &rid, PageNum cur, PageNum &returnedPointer, void *returnedKey);

    // delete given entry
    // 0 - success, 1 - entry not exist
    RC deleteEntry(IXFileHandle &ixFileHandle, const Attribute &attribute, const void *key, const RID &rid, const PageNum &cur);

    void printBTree(IXFileHandle &ixFileHandle, const Attribute &attribute, PageNum &pageId, int level) const;

    // leaf node split function
    // 0: success, 1: insert fail
    RC splitLeafNode(IXFileHandle &ixFileHandle, const Attribute &attribute, const PageNum id, const void* key, const RID &rid, void *returnedKey, PageNum &returnedPointer);
    // key node split function
    void splitKeyNode(IXFileHandle &ixFileHandle, const Attribute &attribute, const PageNum id, void *returnedKey, PageNum &returnedPointer);
    // find which pointer contains the given key
    PageNum findNextNode(IXFileHandle &ixFileHandle, const Attribute &attribute, const PageNum &cur, const void *key);

    // printBTree helper functions;
    // print several spaces;
    void printSpace(int spaceNum) const;
    void printKey(const Attribute &attribute, const void *data) const;
    void printRids(const void* data, PageNum totalLength) const;

public:
    static IndexManager &instance();

    // Create an index file.
    // return 0 - success, 1 - file exists
    RC createFile(const std::string &fileName);

    // Delete an index file.
    // return 0 - success, 1 - file not exists,  other - return by remove()
    RC destroyFile(const std::string &fileName);

    // Open an index and return an ixFileHandle.
    // return: 0 - success, -1 - ixFileHandle already occupied, 1 - file not exists
    RC openFile(const std::string &fileName, IXFileHandle &ixFileHandle);

    // Close an ixFileHandle for an index.
    // return: 0
    RC closeFile(IXFileHandle &ixFileHandle);

    // Insert an entry into the given index that is indicated by the given ixFileHandle.
    RC insertEntry(IXFileHandle &ixFileHandle, const Attribute &attribute, const void *key, const RID &rid);

    // Delete an entry from the given index that is indicated by the given ixFileHandle.
    RC deleteEntry(IXFileHandle &ixFileHandle, const Attribute &attribute, const void *key, const RID &rid);

    // Initialize and IX_ScanIterator to support a range search
    RC scan(IXFileHandle &ixFileHandle,
            const Attribute &attribute,
            const void *lowKey,
            const void *highKey,
            bool lowKeyInclusive,
            bool highKeyInclusive,
            IX_ScanIterator &ix_ScanIterator);

    // Print the B+ tree in pre-order (in a JSON record format)
    void printBtree(IXFileHandle &ixFileHandle, const Attribute &attribute) const;

    static std::string getString(const void* key);
protected:
    IndexManager() = default;                                                   // Prevent construction
    ~IndexManager() = default;                                                  // Prevent unwanted destruction
    IndexManager(const IndexManager &) = default;                               // Prevent construction by copying
    IndexManager &operator=(const IndexManager &) = default;                    // Prevent assignment

};

class IX_ScanIterator {

    static const KeyNumber NotStartRIDIndex = std::numeric_limits<KeyNumber>::max();
    static const KeyNumber NotStartKeyIndex = std::numeric_limits<KeyNumber>::max();

private:
    PageNum currentPage;
    void *currentPageData;
    void *currentKey;
    KeyNumber currentKeyIndex;
    KeyNumber lastRIDIndex;
    KeyNumber currentKeyRIDNumber;
    Attribute attribute;
    void *lowKey, *highKey;
    // passed, no need to release
    IXFileHandle *ixFileHandle;
    bool lowKeyInclusive, highKeyInclusive;

    /* find next existed key */
    // currentPageData need to be assigned first
    bool findNextKey();

    /* compare current key with lowKey and highKey */
    bool currentKeySatisfied();

public:
    // Constructor
    IX_ScanIterator();

    // Destructor
    ~IX_ScanIterator();

    // setup for a new scan
    void setup(IXFileHandle &ixFileHandle,
               const Attribute &attribute,
               const void* lowKey, const void* highKey,
               const bool &lowKeyInclusive, const bool &highKeyInclusive);

    // Get next matching entry
    RC getNextEntry(RID &rid, void *key);

    // Terminate index scan
    RC close();
};

class IXFileHandle {
public:
    static const PageNum NotExistRootPageID = 0;  // 0 is hidden page

private:

    std::fstream *handle;
    PageNum rootPageID;

    /* write hidden page
     *
     * Hidden page format:
     * ┌───────┬──────────────────┬───────────────────┬────────────────────┬─────────────┬────────────────┐
     * │IsInit │ readPageCounter  │ writePageCounter  │ appendPageCounter  │ pageNumber  │  root page id  │
     * └───────┴──────────────────┴───────────────────┴────────────────────┴─────────────┴────────────────┘
     *
     * Return:
     *  0: success
     *  1: fail, currently only when no file is open
     */
    RC writeHiddenPage();

    /* read counters from the hidden page
     * Return:
     *  0: success
     *  1: fail, file not contain a hidden page
     */
    RC readHiddenPage();

public:

    PageNum totalPageNum;

    // variables to keep counter for each operation
    Counter ixReadPageCounter;
    Counter ixWritePageCounter;
    Counter ixAppendPageCounter;

    // Constructor
    IXFileHandle();

    // Destructor
    ~IXFileHandle();

    // Put the current counter values of associated PF FileHandles into variables
    RC collectCounterValues(Counter &readPageCount, Counter &writePageCount, Counter &appendPageCount);

    /* Set file handle */
    // not check if handle is occupied
    void setHandle(std::fstream *f);

    /* Release file handle */
    // not check if handle is occupied
    void releaseHandle();

    /* Return handler status */
    bool isOccupied();

    // append a node page
    // return 0 - success, other -fail
    RC appendNodePage(const void* pageData, PageNum &pageID);

    // Read a node page
    // return 0 - success, other- fail
    RC readNodePage(void *pageData, const PageNum &pageID);

    // Write a node page
    // return 0 - success, other - fail
    RC writeNodePage(const void *pageData, const PageNum &pageID);

    // Get root node page id
    PageNum getRootNodeID() { return rootPageID; };

    // Set root node page id
    void setRootNodeID(const PageNum &rootPageID) { this->rootPageID = rootPageID; };

};

#endif
