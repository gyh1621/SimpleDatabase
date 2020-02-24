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
