#include "rm.h"

RelationManager &RelationManager::instance() {
    static RelationManager _relation_manager = RelationManager();
    return _relation_manager;
}

RelationManager::RelationManager() = default;

RelationManager::~RelationManager() = default;

RelationManager::RelationManager(const RelationManager &) = default;

RelationManager &RelationManager::operator=(const RelationManager &) = default;

RC RelationManager::createCatalog() {
    return -1;
}

RC RelationManager::deleteCatalog() {
    return -1;
}

RC RelationManager::createTable(const std::string &tableName, const std::vector<Attribute> &attrs) {
    return -1;
}

RC RelationManager::deleteTable(const std::string &tableName) {
    return -1;
}

RC RelationManager::getAttributes(const std::string &tableName, std::vector<Attribute> &attrs) {
    if (tableName == SYSTABLE) {
        getTablesAttributes(attrs);
        return 0;
    } else if (tableName == SYSCOLTABLE) {
        getColumnsAttributes(attrs);
        return 0;
    }

    std::string tableFileName;
    TableID tableID;
    RC rc = getTableInfo(tableName, tableID, tableFileName);
    if (rc != 0) return rc;

    std::vector<Attribute> descriptor;
    getColumnsAttributes(descriptor);
    std::vector<std::string> attrNames;
    getDescriptorString(descriptor, attrNames);

    RM_ScanIterator rmsi;
    rc = scan(SYSCOLTABLE, "table-id", EQ_OP, tableID, attrNames, rmsi);
    if (rc != 0) return rc;

    void *data = malloc(TUPLE_TMP_SIZE);
    RID rid;

    while (rmsi.getNextTuple(rid, data) != RM_EOF) {
        Record r(descriptor, data);
        Attribute attr;
        void *data = r.getFieldValue(1);  // column-name
        if (data == nullptr) throw std::bad_alloc();
        // TODO: test
        attr.name = *((std::string *) data);
        free(data);
        data = r.getFieldValue(2);  // column-type
        attr.type = *((AttrType *) data);
        free(data);
        data = r.getFieldValue(3);  // column-length
        attr.length = *((AttrLength *) data);
        free(data);
        attrs.push_back(attr);
    }
}

RC RelationManager::insertTuple(const std::string &tableName, const void *data, RID &rid) {
    return -1;
}

RC RelationManager::deleteTuple(const std::string &tableName, const RID &rid) {
    return -1;
}

RC RelationManager::updateTuple(const std::string &tableName, const void *data, const RID &rid) {
    return -1;
}

RC RelationManager::readTuple(const std::string &tableName, const RID &rid, void *data) {
    return -1;
}

RC RelationManager::printTuple(const std::vector<Attribute> &attrs, const void *data) {
    return -1;
}

RC RelationManager::readAttribute(const std::string &tableName, const RID &rid, const std::string &attributeName,
                                  void *data) {
    return -1;
}

RC RelationManager::scan(const std::string &tableName,
                         const std::string &conditionAttribute,
                         const CompOp compOp,
                         const void *value,
                         const std::vector<std::string> &attributeNames,
                         RM_ScanIterator &rm_ScanIterator) {
    std::string tableFileName;
    TableID tableID;
    RC rc = getTableInfo(tableName, tableID, tableFileName);
    if (rc != 0) return rc;
    std::vector<Attribute> descriptor;
    getAttributes(tableName, descriptor);
    rm_ScanIterator.setUp(tableFileName, conditionAttribute, compOp, value, attributeNames, descriptor);
    return 0;
}

// Extra credit work
RC RelationManager::dropAttribute(const std::string &tableName, const std::string &attributeName) {
    return -1;
}

// Extra credit work
RC RelationManager::addAttribute(const std::string &tableName, const Attribute &attr) {
    return -1;
}


void RM_ScanIterator::setUp(const std::string &tableFileName, const std::string &conditionAttribute, const CompOp compOp,
                            const void *value, const std::vector<std::string> &attributeNames,
                            const std::vector<Attribute> descriptor) {
    RecordBasedFileManager::instance().openFile(tableFileName, this->fileHandle);
    RecordBasedFileManager::instance().scan(
            this->fileHandle, descriptor, conditionAttribute,
            compOp, value, attributeNames, this->rbfm_ScanIterator);
}

RC RM_ScanIterator::getNextTuple(RID &rid, void *data) {
    RC rc = rbfm_ScanIterator.getNextRecord(rid, data);
    if (rc == RBFM_EOF) return RM_EOF;
    else return rc;
}

RC RM_ScanIterator::close() {
    rbfm_ScanIterator.close();
    return RecordBasedFileManager::instance().closeFile(fileHandle);
}
