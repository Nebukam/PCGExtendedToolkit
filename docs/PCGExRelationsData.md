# Relations Data ![Data](https://img.shields.io/badge/Data-cc9b1f)

A new data type that stores relation params definition; and act as data model to read/write relations in relational nodes.

Due to current PCG limitations around datatypes, relation socket data is stored as an *FVector4*:  

| Component 	| Data 	|
|-----------	|--------	|
| X           	| Index of the point this socket is connected to       	|
| Y          	| Type of connection ( 0 = unknown, 1 = One-way, 2 = Shared)       	|
| Z          	| *Reserved*       	|
| W          	| *Reserved*       	|
