#ifndef _rm_h_
#define _rm_h_

#include <string>
#include <vector>

#include "../rbf/rbfm.h"

# define RM_EOF (-1)  // end of a scan operator

# define TUPLE_TMP_SIZE PAGE_SIZE

// RM_ScanIterator is an iterator to go through tuples
class RM_ScanIterator {
private:

public:
    FileHandle fileHandle;

    RBFM_ScanIterator rbfm_ScanIterator;

    RM_ScanIterator() = default;

    ~RM_ScanIterator() = default;

    void setUp(const std::string &tableFileName, const std::string &conditionAttribute,
               CompOp compOp, const void *value, const std::vector<std::string> &attributeNames,
               std::vector<Attribute> descriptor);

    // "data" follows the same format as RelationManager::insertTuple()
    // Return: 0 - success, RM_EOF: end
    RC getNextTuple(RID &rid, void *data);

    // Return: same as RecordBasedFileManager::closeFile
    RC close();
};

// Relation Manager
class RelationManager {
public:
    static RelationManager &instance();

    RC createCatalog();

    RC deleteCatalog();

    RC createTable(const std::string &tableName, const std::vector<Attribute> &attrs);

    RC deleteTable(const std::string &tableName);

    /* Return:
     *  0 - success
     *  -1 - table not exists
     */
    RC getAttributes(const std::string &tableName, std::vector<Attribute> &attrs);

    RC insertTuple(const std::string &tableName, const void *data, RID &rid);

    RC deleteTuple(const std::string &tableName, const RID &rid);

    RC updateTuple(const std::string &tableName, const void *data, const RID &rid);

    RC readTuple(const std::string &tableName, const RID &rid, void *data);

    // Print a tuple that is passed to this utility method.
    // The format is the same as printRecord().
    RC printTuple(const std::vector<Attribute> &attrs, const void *data);

    RC readAttribute(const std::string &tableName, const RID &rid, const std::string &attributeName, void *data);

    // Scan returns an iterator to allow the caller to go through the results one by one.
    // Do not store entire results in the scan iterator.
    // Return:
    //   0: success
    //  -1: table not exists, same as getTableInfo return code
    RC scan(const std::string &tableName,
            const std::string &conditionAttribute,
            const CompOp compOp,                  // comparison type such as "<" and "="
            const void *value,                    // used in the comparison
            const std::vector<std::string> &attributeNames, // a list of projected attributes
            RM_ScanIterator &rm_ScanIterator);

// Extra credit work (10 points)
    RC addAttribute(const std::string &tableName, const Attribute &attr);

    RC dropAttribute(const std::string &tableName, const std::string &attributeName);

protected:
    RelationManager();                                                  // Prevent construction
    ~RelationManager();                                                 // Prevent unwanted destruction
    RelationManager(const RelationManager &);                           // Prevent construction by copying
    RelationManager &operator=(const RelationManager &);                // Prevent assignment

};

#endif