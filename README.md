# LargeDatagenerator
Use OCI's Direct-Path-Load Engine to generate large amounts of data on tables of Oracle Database at high speed

## Description
Create a target record to be used for performance testing of mass data.
The value of the record to be created is mechanically generated, so it has no meaning.
You can call OCI's Direct-Path-Load Engine with multithreading and load it into tables of Oracle Database at high speed.

## Demo

## Requirement
1.Oracle Database

2.Microsoft VisualStudio

## Usage
    C:\Users\XXX\source\repos\LargeDataGenerator\x64\Debug>LargeDataGenerator.exe
    Usage: LargeDataGenerator.exe user/passwd@service_name table_name create_count parallel

    C:\Users\XXX\source\repos\LargeDataGenerator\x64\Debug>LargeDataGenerator.exe SCOTT/SCOTT@ORCL PARQUET_TEST 1000 4
    2018-05-18 09:55:57 [main] INFO:  >>>execute start
    2018-05-18 09:55:57 [main] INFO:  OCIEnvCreate start
    2018-05-18 09:55:57 [main] INFO:  OCIHandleAlloc(OCIErrorhandle) start
    2018-05-18 09:55:57 [main] INFO:  OCIHandleAlloc(OCIServer) start
    2018-05-18 09:55:57 [main] INFO:  OCIHandleAlloc(OCISvcCtx) start
    *******************************************************************************
    * LargeDataGenerator startup.

    *******************************************************************************
    ■インスタンス名：[ORCL]
    ■スキーマ名　　：[SCOTT]
    ■テーブル名　　：[PARQUET_TEST]
    ■項目数　　　　：         7
    ■総ロード件数　：      1000
    ■並列スレッド数：         4
    処理を続行しますか？(Y/N)>y
    ...
    *******************************************************************************
    * LargeDataGenerator finishedd.

    *******************************************************************************

## Install
1.Create a new Console Application project in MicroSoft Visual Studio

2.Add the following directories to the include directory
    
    %ORACLE_HOME%\oci\include

3.Add the following directories to the library directory

    %ORACLE_HOME%\rdbms\lib
    %ORACLE_HOME%\oci\lib
    %ORACLE_HOME%\oci\lib\msvc
    %ORACLE_HOME%\oci\lib\msvc\vc14

4.Build a project and generate LargeDataGenerator.exe


## Contribution

## Licence
This software is released under the MIT License, see LICENSE.txt.

## Author
[Manabu-Kobatake](https://github.com/Manabu-Kobatake)
