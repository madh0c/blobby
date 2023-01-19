Your Task
Your task in this assignment is to write a C program blobby.c, a file archiver.

The file archives in this assignment are called blobs.
Each blob contains one or more blobettes.
Each blobette records one file system object.
Their format is described below.

blobby.c should be able to:

- list the contents of a blob (subset 0),
- list the permissions of files in a blob (subset 0),
- list the size (number of bytes) of files in a blob (subset 0),
- check the blobette magic number (subset 0),
- extract files from a blob (subset 1),
- check blobette integrity (hashes) (subset 1),
- set the file permissions of files extracted from a blob (subset 1),
- create a blob from a list of files (subset 2),
- list, extract, and create blobs that include directories (subset 3),
- list, extract, and create blobs that are compressed (subset 4).
