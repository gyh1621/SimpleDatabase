## 1. Basic information
- Team #: 14
- Github Repo Link: https://github.com/UCI-Chenli-teaching/cs222p-winter20-team-14
- Student 1 UCI NetID: yuhangg4
- Student 1 Name: Yuhang Guo
- Student 2 UCI NetID: lijies2
- Student 2 Name: Lijie Song


## 2. Catalog information about Index
##### Show your catalog information about an index (tables, columns).
We have a extra system catalog called Index to store index information;
Each index has two field in Index catalog: index-name and index-filename;
In Tables catalog, it has a record with table id 3 referring to Index catalog;
In columns catalog, it contains attribute info of Index catalog; 

## 3. Block Nested Loop Join (If you have implemented this feature)
##### Describe how your block nested loop join works (especially, how you manage the given buffers.)
We created a in-memory hash table to hold records of a block, whose keys are conditional
attributes(all types converted to string) and values are vectors holding pointers to raw record data.
Once total length of raw record data in hash table reaches `numPages * PAGE_SIZE`, we stop the "reading-block"
process and start joining. In implementation, we read the first block in the constructor.

When joining, each time we fetch records from right relation until the conditional attribute of a record exists
in hash table and we return the joined record. When we get `QE_EOF` fetching records from right relation, we
read next block, constructing a new in-memory hash table and repeat above joining process until we get `QE_EOF`
from left relation.


## 4. Index Nested Loop Join (If you have implemented this feature)
##### Describe how your index nested join works.
Initially read the first record from left table, pass the key value to the right index iterator;
For each record returned from the index iterator, combine it with the left record and output the combined record;
Once the right iterator gets the end, read next record from left table and reset right iterator;
Once the left table gets the end, the index nested join is done;

## 5. Grace Hash Join (If you have implemented this feature)
##### Describe how your grace hash join works (especially, in-memory structure).
In constructor, we create `numPartitions` partitions as `rbfm` files using our hash function.

Then, in the probing phase, we create a in-memory hash table for a smaller partition of R or S,
whose keys are conditional attribute(all types converted to string) and values are vectors holding
pointers to raw record data. And we create a `RBFM_ScanIterator` for the larger partition.

When joining, we get records from scan iterator until a conditional attribute of a records has
matches in the hash table and we return the joined record. We load next pair partitions of R and S
when `RBFM_ScanIterator` return `RBFM_EOF` and repeat the above process.

The partition files will be deleted after `GHJoin` has no results(`QE_EOF`).


## 6. Aggregation
##### Describe how your aggregation (basic, group-based hash) works.

**Basic**

We maintain two variables `count` and `returnedVal` when scanning the input iterator.

`count` is the number of records whose conditional attributes are not null. `returnedVal` is the result of op.
In extreme situations, like all records are null, we just return `QE_EOF`.

Because basic operations only have one result, so if have a result, after calling `getNextTuple` once,
the following repeated calls of `getNextTuple` will always return `QE_EOF`.

**Group**

We maintain two hash tables `group` and `groupCount` when scanning the input iterator.

`group`'s keys are group attribute values(all converted to string), values are results of op.
`groupCount` is only used when op is `AVG`, whose keys are group attribute values and values are
number of records in a group.
In extreme situations, like a group's all records are null or no group, we skip that group or return `QE_EOF`.

The `getNextTuple` will always return `QE_EOF` when all groups' results have been returned.


## 7. Implementation Detail
##### Have you added your own source file (.cc or .h)?
No.
##### Have you implemented any optional features? Then, describe them here.
We implemented Grace Hash Join and Group-based Hash Aggregation
##### Other implementation details:
Index catalog implementation:
    We add an extra system catalog to store meta data of index file;
    Records in Index catalog contains two fields: index-name, index-filename;
    Index name is in the format of relation.attribute;
    Meta info of Index catalog is contained in Tables and Columns catalog;

## 6. Other (optional)
##### Freely use this section to tell us about things that are related to the project 4, but not related to the other sections (optional)