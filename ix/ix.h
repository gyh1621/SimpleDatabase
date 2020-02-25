#ifndef _ix_h_
#define _ix_h_

#include <vector>
#include <string>

#include "../rbf/rbfm.h"
#include "nodepage.h"

# define IX_EOF (-1)  // end of the index scan

class IX_ScanIterator;

class IXFileHandle;

class IndexManager {
private:
    // insert new Entry
    // if returnedKey key is not null, it means split happened before,
    // a new key and its right  pointer need to be added
    // 0 - success, other - rid exists
    RC insertEntry(IXFileHandle &ixFileHandle, const Attribute &attribute, const void *key, const RID &rid, PageNum cur, PageNum &returnedPointer, void *returnedKey);

    // delete given entry
    // 0 - success, 1 - entry not exist
    RC deleteEntry(IXFileHandle &ixFileHandle, const Attribute &attribute, const void *key, const RID &rid, PageNum cur);

    void printBTree(IXFileHandle &ixFileHandle, const Attribute &attribute, PageNum &pageId, int level) const;

    // leaf node split function
    // 0: success, 1: insert fail
    RC splitLeafNode(IXFileHandle &ixFileHandle, const Attribute &attribute, const PageNum id, const void* key, const RID &rid, void *MiddleKey, PageNum &newNodeId);
    // key node split function
    void splitKeyNode(IXFileHandle &ixFileHandle, const Attribute &attribute, const PageNum id, void *MiddleKey, PageNum &newNodeId);
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
    RC createFile(const std::string &fileName);

    // Delete an index file.
    RC destroyFile(const std::string &fileName);

    // Open an index and return an ixFileHandle.
    RC openFile(const std::string &fileName, IXFileHandle &ixFileHandle);

    // Close an ixFileHandle for an index.
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
public:
    // Constructor
    IX_ScanIterator();

    // Destructor
    ~IX_ScanIterator();

    // Get next matching entry
    RC getNextEntry(RID &rid, void *key);

    // Terminate index scan
    RC close();
};

class IXFileHandle {
public:

    // variables to keep counter for each operation
    unsigned ixReadPageCounter;
    unsigned ixWritePageCounter;
    unsigned ixAppendPageCounter;

    // Constructor
    IXFileHandle();

    // Destructor
    ~IXFileHandle();

    // Put the current counter values of associated PF FileHandles into variables
    RC collectCounterValues(unsigned &readPageCount, unsigned &writePageCount, unsigned &appendPageCount);

    // Create a node page
    // return 0 - success, other -fail
    RC createNodePage(void* pageData, PageNum &pageID, bool &isLeaf);

    // Read a node page
    // return 0 - success, other- fail
    RC readNodePage(void *pageData, const PageNum &pageID);

    // Write a node page
    // return 0 - success, other - fail
    RC writeNodePage(const void *pageData, const PageNum &pageID);

};

#endif
