/********************************************************************************/
/*	LargeDataGenerator(for Oracle)												*/
/********************************************************************************/

#include "stdafx.h"
using namespace std;

#define LOAD_UNIT 1000
#define DATE_FMT "YYYY-MM-DD HH24:MI:SS"
#define TIMESTAMP_FMT "YYYY-MM-DD HH24:MI:SS.FF3"

/********************************************************************************/
/*	Oracle環境情報クラス														*/
/********************************************************************************/
class OracleEnvs
{
public:
	char	userName[30 + 1];
	char	passwd[30 + 1];
	char	svcName[30 + 1];
	char	tblName[30 + 1];
	OracleEnvs()
	{
		memset(this->userName, '\0', sizeof(this->userName));
		memset(this->passwd, '\0', sizeof(this->passwd));
		memset(this->svcName, '\0', sizeof(this->svcName));
		memset(this->tblName, '\0', sizeof(this->tblName));
	}
};

/********************************************************************************/
/*	テーブル項目定義クラス														*/
/********************************************************************************/
class ColumnAttrs
{
public:
	char	colName[30 + 1];
	ub2		colPos;
	ub2		extendType;
	text	*colDateMask;
	ub4		fieldSize;
	char	dataType;
	char	dataTypes[30 + 1];
	bool	isPrimaryKey;
	ColumnAttrs()
	{
		memset(this->colName, '\0', sizeof(this->colName));
		this->colPos		=	0;
		this->extendType	=	SQLT_CHR;
		this->colDateMask	=	nullptr;
		this->fieldSize		=	0;
		this->dataType		=	'\0';
		memset(this->dataTypes, '\0', sizeof(this->dataTypes));
		this->isPrimaryKey	=	false;
	}
};

/********************************************************************************/
/*	テーブル定義クラス															*/
/********************************************************************************/
class TableAttrs
{
public:
	text	*tblOwner;
	text	*tblName;
	text	*subTblName;
	ub2		colCount;
	text	*defaultDateMask;
	ub1		noLog;
	ub4		transferSize;
	ub2		recordSize;
	ub1		parallel;
	ub2		input;
	ub2		indexSkip;
	size_t	allocSize;
	TableAttrs()
	{
		this->tblOwner			 = nullptr;
		this->tblName			 = nullptr;
		this->subTblName		 = nullptr;
		this->colCount			 = (ub2)0;
		this->defaultDateMask	 = nullptr;
		this->noLog				 = (ub1)1;
		this->transferSize		 = (ub4)256 * 1024;
		this->recordSize		 = (ub2)0;
		this->parallel			 = (ub1)1;
		this->input				 = (ub2)OCI_DIRPATH_INPUT_TEXT;
		this->indexSkip			 = (ub1)OCI_DIRPATH_INDEX_MAINT_SKIP_ALL;
//		this->indexSkip			 = (ub1)OCI_DIRPATH_INDEX_MAINT_SKIP_UNUSABLE;
		this->allocSize			 = (size_t)0;
	}
};

/********************************************************************************/
/*	ダイレクトパスロードクラス													*/
/********************************************************************************/
class DirectPathLoad 
{
public:
	OracleEnvs			*oraEnv;
	TableAttrs			*tblAttr;
	ColumnAttrs			*colAttr;
	vector<string>		defaultsValue;
private:
	string				threadTag;
	unsigned int		startNo;
	unsigned int		currentNo;
	unsigned int		count;
	OCIEnv				*ociEnv;
	OCIServer			*ociSvr;
	OCIError			*ociErr;
	OCISvcCtx			*ociCtx;
	OCIStmt				*ociStmt;
	OCISession			*ociSes;
	OCIDirPathCtx		*dpCtx;
	OCIDirPathColArray	*dpca;
	OCIDirPathStream	*dpstr;
	OCIParam			*colLstDesc;
	int					dpRows;
	int					dpCols;
	char				*prcrds;
public:
	/********************************************************************************/
	/*	コンストラクタ																*/
	/********************************************************************************/
	DirectPathLoad(
		char *userName,
		char *passwd,
		char *svcName,
		char *tblName
	)
	{
		this->threadTag = "[main]";
		this->oraEnv = new OracleEnvs();
		strcpy_s(this->oraEnv->userName	, userName);
		strcpy_s(this->oraEnv->passwd	, passwd);
		strcpy_s(this->oraEnv->svcName	, svcName);
		strcpy_s(this->oraEnv->tblName	, tblName);
		this->tblAttr = new TableAttrs();

		this->colAttr = nullptr;
		this->startNo = 0;
		this->currentNo = 0;
		this->count = 0;

		this->ociEnv = nullptr;
		this->ociSvr = nullptr;
		this->ociErr = nullptr;
		this->ociCtx = nullptr;
		this->ociStmt = nullptr;
		this->ociSes = nullptr;
		this->dpCtx = nullptr;
		this->dpca = nullptr;
		this->dpstr = nullptr;
		this->colLstDesc = nullptr;
		this->dpRows = 0;
		this->dpCols = 0;
		this->prcrds = nullptr;
	}
	/********************************************************************************/
	/*	コンストラクタ(スレッド用)													*/
	/********************************************************************************/
	DirectPathLoad(
		OracleEnvs		*oraEnvs,
		int				threadNo,
		unsigned long	nStartNo,
		unsigned long	nCount,
		TableAttrs		*tblAttrs,
		ColumnAttrs		*colAttrs,
		vector<string>	defaults
	)
	{
		stringstream tmp;
		tmp << "[Thread-" << setw(2) << setfill('0') << threadNo << "]";
		this->threadTag		 = tmp.str();
		this->oraEnv		 = oraEnvs;
		this->tblAttr		 = tblAttrs;
		this->colAttr		 = colAttrs;

		this->startNo		 = nStartNo;
		this->currentNo		 = nStartNo;
		this->count			 = nCount;
		this->defaultsValue	 = defaults;

		this->ociEnv = nullptr;
		this->ociSvr = nullptr;
		this->ociErr = nullptr;
		this->ociCtx = nullptr;
		this->ociStmt = nullptr;
		this->ociSes = nullptr;
		this->dpCtx = nullptr;
		this->dpca = nullptr;
		this->dpstr = nullptr;
		this->colLstDesc = nullptr;
		this->dpRows = 0;
		this->dpCols = 0;
		this->prcrds = nullptr;
	}

	/********************************************************************************/
	/*	デストラクタ																*/
	/********************************************************************************/
	~DirectPathLoad()
	{
		this->logInfo(1, "*** destructor start ***");
		// Oracle環境定義クラスの解放
		if (this->oraEnv)
		{
			delete this->oraEnv;
			this->oraEnv = nullptr;
		}
		// テーブル定義クラスの解放
		if (this->tblAttr)
		{
			delete this->tblAttr;
			this->tblAttr = nullptr;
		}
		// テーブル項目定義クラスの解放
		if (this->colAttr)
		{
			delete [] this->colAttr;
			this->colAttr = nullptr;
		}
		// OCI関連リソースの解放
		this->cleanupOci();
		this->logInfo(1, "*** destructor end ***");
	}

private:
	/********************************************************************************/
	/*	OCI関連リソースの解放(スレッド単位)											*/
	/********************************************************************************/
	void cleanupOci()
	{
		this->logInfo(1, "*** cleanupOci start ***");
		sword status = OCI_SUCCESS;

		//入力レコード配列の解放
		if (this->prcrds)
		{
			delete[] this->prcrds;
			this->prcrds = nullptr;
		}
		//ダイレクトパスストリームの解放
		if (this->dpstr)
		{
			(void)OCIHandleFree(
					(dvoid *)this->dpstr,
					(ub4)OCI_HTYPE_DIRPATH_STREAM
			);
			this->dpstr = nullptr;
		}
		//ダイレクトパス列配列の解放
		if (this->dpca)
		{
			(void)OCIHandleFree(
				(dvoid *)this->dpca,
				(ub4)OCI_HTYPE_DIRPATH_COLUMN_ARRAY
			);
			this->dpca = nullptr;
		}
		//SQLステートメントハンドラの解放
		if (this->ociStmt)
		{
			(void)OCIHandleFree(
				(dvoid *)this->ociStmt,
				(ub4)OCI_HTYPE_STMT
			);
			this->ociStmt = nullptr;
		}
		//SQLステートメントハンドラの解放
		if (this->ociStmt)
		{
			if (status = OCIHandleFree(
				(dvoid *)this->ociStmt,
				(ub4)OCI_HTYPE_STMT
			))
			{
				this->logErr(1, "OCIHandleFree(OCI_HTYPE_STMT) failed.");
				this->checkErr(status);
			}
			this->ociStmt = nullptr;
		}
		//ダイレクトパスコンテキストハンドラの解放
		if (this->dpCtx)
		{
			if (status = OCIHandleFree(
				(dvoid *)this->dpCtx,
				(ub4)OCI_HTYPE_DIRPATH_CTX
			))
			{
				this->logErr(1, "OCIHandleFree(OCI_HTYPE_DIRPATH_CTX) failed.");
				this->checkErr(status);
			}
			this->dpCtx = nullptr;
		}
		//OCIセッションの切断
		if (this->ociSes)
		{
			if (status = OCISessionEnd(
				this->ociCtx,
				this->ociErr,
				this->ociSes,
				(ub4)OCI_DEFAULT
			))
			{
				this->logErr(1, "OCISessionEnd failed.");
				this->checkErr(status);
			}
			this->ociSes = nullptr;
		}
		//サービスコンテキストハンドラの解放
		if (this->ociCtx)
		{
			if (status = OCIHandleFree(
				(dvoid *)this->ociCtx,
				(ub4)OCI_HTYPE_SVCCTX
			))
			{
				this->logErr(1, "OCIHandleFree(OCI_HTYPE_SVCCTX) failed.");
				this->checkErr(status);
			}
			this->ociCtx = nullptr;
		}
		//サービスハンドラのデタッチ＆解放
		if (this->ociSvr)
		{
			if (status = OCIServerDetach(
				this->ociSvr,
				this->ociErr,
				(ub4)OCI_DEFAULT
			))
			{
				this->logErr(1, "OCIServerDetach failed.");
				this->checkErr(status);
			}
			if (status = OCIHandleFree(
				(dvoid *)this->ociSvr,
				OCI_HTYPE_SERVER
			))
			{
				this->logErr(1, "OCIHandleFree(OCI_HTYPE_SERVER) failed.");
				this->checkErr(status);
			}
			this->ociSvr = nullptr;
		}
		//エラーハンドラの解放
		if (this->ociErr)
		{
			(void)OCIHandleFree(
				(dvoid *)this->ociErr,
				(ub4)OCI_HTYPE_ERROR
			);
			this->ociErr = nullptr;
		}
		//OCI終了処理
		if (this->ociEnv)
		{
			if (status = OCITerminate((ub4)OCI_DEFAULT))
			{
				this->logErr(1, "OCITerminate is failed.");
			}
			this->ociEnv = nullptr;
		}
	}

	/********************************************************************************/
	/*	OCIエラーチェック															*/
	/********************************************************************************/
	void checkErr(sword status)
	{
		stringstream	errInf;
		text			errBuf[512];
		sb4				errCode = (sb4)0;
		memset(errBuf, '\0', sizeof(errBuf));

		switch (status)
		{
		case OCI_SUCCESS:
			break;
		case OCI_SUCCESS_WITH_INFO:
			errInf << "OCI_SUCCESS_WITH_INFO status=" << status;
			break;
		case OCI_NEED_DATA:
			errInf << "OCI_NEED_DATA status=" << status;
			break;
		case OCI_ERROR:
			errInf << "OCI_ERROR status=" << status << ", detail=[";
			if (this->ociErr)
			{
				(void)OCIErrorGet(
					(void *)this->ociErr,
					(ub4)1,
					(text *)nullptr,
					&errCode,
					errBuf,
					sizeof(errBuf),
					(ub4)OCI_HTYPE_ERROR
				);
				errInf << errBuf;
			}
			break;
		case OCI_INVALID_HANDLE:
			errInf << "OCI_INVALID_HANDLE status=" << status;
			break;
		case OCI_STILL_EXECUTING:
			errInf << "OCI_STILL_EXECUTING status=" << status;
			break;
		case OCI_CONTINUE:
			errInf << "OCI_CONTINUE status=" << status;
			break;

		}
		if (errInf.str().size() > 0)
		{
			logErr(1, errInf.str().c_str());
		}
	}
	/********************************************************************************/
	/*	現在時刻取得																*/
	/********************************************************************************/
	const string getCurrentTime()
	{
		stringstream ss;
		struct tm tmi;
		const time_t tmt = time(nullptr);
		memset(&tmi, '\0', sizeof(tmi));

		 if( localtime_s(&tmi, &tmt) ) return "0000-00-00 00:00:00";

		 ss << setw(4) << setfill('0') << tmi.tm_year + 1900 << '-'
			 << setw(2) << setfill('0') << (tmi.tm_mon + 1) << '-'
			 << setw(2) << setfill('0') << tmi.tm_mday << " "
			 << setw(2) << setfill('0') << tmi.tm_hour << ':'
			 << setw(2) << setfill('0') << tmi.tm_min << ':'
			 << setw(2) << setfill('0') << tmi.tm_sec << " ";

		 return ss.str();
	}
	/********************************************************************************/
	/*	ログ出力(COMMON)															*/
	/********************************************************************************/
	void log(bool error, string value)
	{
		cout
			<< this->getCurrentTime()
			<< this->threadTag
			<< " "
			<< (error ? "ERROR: " : "INFO:  ")
			<< value
			<< endl;
	}

	/********************************************************************************/
	/*	ログ出力(INFO)																*/
	/********************************************************************************/
	void logInfo(int num, ...)
	{
		va_list	vlist;
		va_start(vlist, num);

		stringstream val;
		for (int i = 0; i < num; i++)
		{
			char *arg = va_arg(vlist, char *);
			if (arg) val << arg;
		}
		va_end(vlist);
		this->log(false, val.str());
	}

	/********************************************************************************/
	/*	ログ出力(ERROR)																*/
	/********************************************************************************/
	void logErr(int num, ...)
	{
		va_list	vlist;
		va_start(vlist, num);

		stringstream val;
		for (int i = 0; i < num; i++)
		{
			char *arg = va_arg(vlist, char *);
			if (arg) val << arg;
		}
		va_end(vlist);
		this->log(true, val.str());
	}
	/********************************************************************************/
	/*	OCI基本ハンドラ初期化														*/
	/********************************************************************************/
	bool ociInit()
	{
		bool result = false;
		sword status = OCI_SUCCESS;
		// OCI環境生成
		this->logInfo(1, "OCIEnvCreate start");
		if (status = OCIEnvCreate(
			(OCIEnv **)&(this->ociEnv),
			OCI_THREADED,
			(void *)0,
			0,
			0,
			0,
			(size_t)0,
			(void **)0
		)) 
		{
			this->logErr(1,"OCIEnvCreate - error : ");
			goto finally;
		}

		// エラーハンドラ割り当て
		this->logInfo(1, "OCIHandleAlloc(OCIErrorhandle) start");
		if (status = OCIHandleAlloc(
				this->ociEnv,
				(dvoid **)&(this->ociErr),
				OCI_HTYPE_ERROR,
				(size_t)0,
				(void **)0
		))
		{
			this->logErr(1, "OCIHandleAlloc(OCIErrorhandle) - error : ");
			goto finally;
		}
		// サーバハンドラ割り当て
		this->logInfo(1, "OCIHandleAlloc(OCIServer) start");
		if (status = OCIHandleAlloc(
			this->ociEnv,
			(dvoid **)&(this->ociSvr),
			OCI_HTYPE_SERVER,
			(size_t)0,
			(void **)0
		))
		{
			this->logErr(1, "OCIHandleAlloc(OCIServer) - error : ");
			goto finally;
		}
		// サービスコンテキストハンドラ割り当て
		this->logInfo(1, "OCIHandleAlloc(OCISvcCtx) start");
		if (status = OCIHandleAlloc(
			this->ociEnv,
			(dvoid **)&(this->ociCtx),
			OCI_HTYPE_SVCCTX,
			(size_t)0,
			(void **)0
		))
		{
			this->logErr(1, "OCIHandleAlloc(OCISvcCtx) - error : ");
			goto finally;
		}

		result = true;
finally:
		return result;
	}
	/********************************************************************************/
	/*	Oracle環境情報取得															*/
	/********************************************************************************/
	bool getOracleEnvs()
	{
		this->tblAttr->tblOwner = (text *)this->oraEnv->userName;
		this->tblAttr->tblName = (text *)this->oraEnv->tblName;
		return true;
	}
	/********************************************************************************/
	/*	データベース接続															*/
	/********************************************************************************/
	bool connectDB()
	{
		bool result = false;
		sword status = OCI_SUCCESS;
		//データベース接続記述子の設定
		if (status = OCIServerAttach(
			this->ociSvr,
			this->ociErr,
			(text *)this->oraEnv->svcName,
			(sb4)strlen(this->oraEnv->svcName),
			(ub4)OCI_DEFAULT
		))
		{
			this->logErr(1, "OCIServerAttach - error : ");
			goto finally;
		}
		//サービスコンテキストにサーバ属性を設定
		if (status = OCIAttrSet(
			(dvoid *)this->ociCtx,
			(ub4)OCI_HTYPE_SVCCTX,
			(dvoid *)this->ociSvr,
			(ub4)0,
			(ub4)OCI_ATTR_SERVER,
			this->ociErr
		))
		{
			this->logErr(1, "OCIAttrSet(OCI_ATTR_SERVER) - error : ");
			goto finally;
		}
		//セッションハンドル割り当て
		if (status = OCIHandleAlloc(
			(dvoid *)this->ociEnv,
			(dvoid **)&(this->ociSes),
			(ub4)OCI_HTYPE_SESSION,
			(size_t)0,
			(dvoid **)0
		))
		{
			this->logErr(1, "OCIHandleAlloc(OCI_HTYPE_SESSION) - error : ");
			goto finally;
		}
		//セッションハンドルユーザ名設定
		if (status = OCIAttrSet(
			(dvoid *)this->ociSes,
			(ub4)OCI_HTYPE_SESSION,
			(dvoid *)this->oraEnv->userName,
			(ub4)strlen(this->oraEnv->userName),
			(ub4)OCI_ATTR_USERNAME,
			this->ociErr
		))
		{
			this->logErr(1, "OCIAttrSet(OCI_ATTR_USERNAME) - error : ");
			goto finally;
		}
		//セッションハンドルパスワード設定
		if (status = OCIAttrSet(
			(dvoid *)this->ociSes,
			(ub4)OCI_HTYPE_SESSION,
			(dvoid *)this->oraEnv->passwd,
			(ub4)strlen(this->oraEnv->passwd),
			(ub4)OCI_ATTR_PASSWORD,
			this->ociErr
		))
		{
			this->logErr(1, "OCIAttrSet(OCI_ATTR_PASSWORD) - error : ");
			goto finally;
		}
		//セッション開始
		if (status = OCISessionBegin(
			this->ociCtx,
			this->ociErr,
			this->ociSes,
			(ub4)OCI_CRED_RDBMS,
			(ub4)OCI_DEFAULT
		))
		{
			this->logErr(1, "OCISessionBegin - error : ");
			goto finally;
		}
		//サービスコンテキストにセッション属性を設定
		if (status = OCIAttrSet(
			(dvoid *)this->ociCtx,
			(ub4)OCI_HTYPE_SVCCTX,
			(dvoid *)this->ociSes,
			(ub4)0,
			(ub4)OCI_ATTR_SESSION,
			this->ociErr
		))
		{
			this->logErr(1, "OCIAttrSet(OCI_ATTR_SESSION) - error : ");
			goto finally;
		}
		//SQLステートメントハンドル割り当て
		if (status = OCIHandleAlloc(
			(dvoid *)this->ociEnv,
			(dvoid **)&(this->ociStmt),
			(ub4)OCI_HTYPE_STMT,
			(size_t)0,
			(dvoid **)0
		))
		{
			this->logErr(1, "OCIHandleAlloc(OCI_HTYPE_STMT) - error : ");
			goto finally;
		}

		result = true;
finally:
		this->checkErr(status);
		return result;
	}

	/**************************************************************************
	* Oracleロード対象テーブル　メタ情報取得（項目桁数の定義）
	**************************************************************************/
	bool getOracleMetaInfo()
	{
		bool ret = false;
		sword status = OCI_SUCCESS;
		vector<tuple<dvoid *, size_t, ub2, string>> hosts;
		OraText *getCntSql = (OraText *)"SELECT COUNT(*) FROM USER_TAB_COLUMNS WHERE TABLE_NAME = :1";
		OraText *getMetaSql =
			(OraText *)"SELECT													 \
					utc.COLUMN_NAME,											 \
					(															 \
						CASE													 \
							WHEN utc.DATA_TYPE = 'DATE'  THEN 'D'				 \
							WHEN utc.DATA_TYPE LIKE 'TIMESTAMP%' THEN 'T'		 \
							WHEN utc.DATA_TYPE = 'NUMBER'  THEN 'N'				 \
							ELSE 'S'											 \
						END														 \
					) DATA_TYPE,												 \
					(															 \
						CASE													 \
							WHEN utc.DATA_TYPE = 'DATE'			 THEN 20		 \
							WHEN utc.DATA_TYPE LIKE 'TIMESTAMP%' THEN 23		 \
							WHEN utc.DATA_TYPE = 'NUMBER' THEN DATA_PRECISION	 \
							ELSE DATA_LENGTH									 \
						END														 \
					) DATA_TYPE,												 \
					DECODE(ucs.CONSTRAINT_TYPE,'P',1,0) ISPRIMARYKEY			 \
				FROM															 \
					USER_TAB_COLUMNS	utc										 \
					LEFT OUTER JOIN												 \
						(SELECT													 \
							ucc.TABLE_NAME,										 \
							ucc.COLUMN_NAME,									 \
							ucc.CONSTRAINT_NAME,								 \
							ucn.CONSTRAINT_TYPE									 \
						 FROM													 \
							USER_CONS_COLUMNS  ucc								 \
							INNER JOIN USER_CONSTRAINTS  ucn					 \
							ON													 \
								ucc.CONSTRAINT_NAME = ucn.CONSTRAINT_NAME		 \
						 WHERE													 \
							ucn.CONSTRAINT_TYPE = 'P'							 \
						) ucs													 \
						ON														 \
								utc.TABLE_NAME = ucs.TABLE_NAME(+)				 \
							AND													 \
								utc.COLUMN_NAME = ucs.COLUMN_NAME(+)			 \
				WHERE															 \
						utc.TABLE_NAME=:1										 \
				ORDER BY														 \
					utc.COLUMN_ID												 \
		";
		OCIBind			*pbnd = (OCIBind *)0;
		ColumnAttrs		*pdt = nullptr;
		int				lenSummary = 0;
		int				colCnt = 0;
		text			*pBind1 = (text *)this->oraEnv->tblName;

		//--------------------------------------------------------
		// カラム数取得用SQLの解析
		//--------------------------------------------------------
		if (status = OCIStmtPrepare(
			this->ociStmt,
			this->ociErr,
			getCntSql,
			(ub4)strlen((const char *)getCntSql),
			(ub4)OCI_NTV_SYNTAX,
			(ub4)OCI_DEFAULT
		)) {
			this->checkErr(status);
			goto finally;
		}

		//--------------------------------------------------------
		// WHERE条件の設定
		//--------------------------------------------------------
		if (status = OCIBindByPos(
			this->ociStmt,
			&pbnd,
			this->ociErr,
			(ub4)1,
			(dvoid *)pBind1,
			(sword)strlen((char *)pBind1),
			SQLT_CHR,
			(dvoid *)0,
			(ub2 *)0,
			(ub2 *)0,
			(ub4)0,
			(ub4 *)0,
			(ub4)OCI_DEFAULT
		))
		{
			this->checkErr(status);
			goto finally;
		}
		//--------------------------------------------------------
		// 結果格納変数設定
		//--------------------------------------------------------
		OCIDefine		**pdfnc = (OCIDefine **)malloc(sizeof(OCIDefine *));
		if (status = OCIDefineByPos(
			this->ociStmt,
			&(pdfnc[0]),
			this->ociErr,
			(ub4)1,
			(dvoid *)&colCnt,
			(sword)sizeof(colCnt),
			(ub2)SQLT_INT,
			(void *)0,
			(ub2 *)0,
			(ub2 *)0,
			(ub4)OCI_DEFAULT
		))
		{
			this->checkErr(status);
			goto finally;
		}
		//--------------------------------------------------------
		// 準備済みのSQL文を実行する
		//--------------------------------------------------------
		if (status = OCIStmtExecute(
			this->ociCtx,
			this->ociStmt,
			this->ociErr,
			(ub4)1,
			(ub4)0,
			(const OCISnapshot *)nullptr,
			(OCISnapshot *)nullptr,
			(ub4)OCI_DEFAULT
		))
		{
			this->checkErr(status);
			goto finally;
		}

		this->tblAttr->colCount = colCnt;

		//--------------------------------------------------------
		// 項目定義取得用領域準備
		//--------------------------------------------------------
		OCIDefine** pdfn = (OCIDefine **)calloc(colCnt, sizeof(OCIDefine *));
		memset(pdfn, '\0', sizeof(OCIDefine *)*colCnt);
		this->colAttr = new ColumnAttrs[colCnt];
		pdt = this->colAttr;

		//--------------------------------------------------------
		// 項目定義取得用SQLの解析
		//--------------------------------------------------------
		if (status = OCIStmtPrepare(
			this->ociStmt,
			this->ociErr,
			getMetaSql,
			(ub4)strlen((const char *)getMetaSql),
			(ub4)OCI_NTV_SYNTAX,
			(ub4)OCI_DEFAULT
		)) {
			this->checkErr(status);
			goto finally;
		}

		//--------------------------------------------------------
		// WHERE条件の設定
		//--------------------------------------------------------
		if (status = OCIBindByPos(
			this->ociStmt,
			&pbnd,
			this->ociErr,
			(ub4)1,
			(dvoid *)pBind1,
			(sword)strlen((char *)pBind1),
			SQLT_CHR,
			(dvoid *)0,
			(ub2 *)0,
			(ub2 *)0,
			(ub4)0,
			(ub4 *)0,
			(ub4)OCI_DEFAULT
		))
		{
			this->checkErr(status);
			goto finally;
		}
		//--------------------------------------------------------
		// 結果格納変数設定
		//--------------------------------------------------------
		hosts.push_back(make_tuple((dvoid *)pdt->colName		, sizeof(pdt->colName) - 1		,SQLT_CHR	,	"colName")		);
		hosts.push_back(make_tuple((dvoid *)pdt->dataTypes		, sizeof(pdt->dataTypes) - 1	,SQLT_CHR	,	"dataTypes")	);
		hosts.push_back(make_tuple((dvoid *)&(pdt->fieldSize)	, sizeof(pdt->fieldSize)		,SQLT_INT	,   "fieldSize")	);
		hosts.push_back(make_tuple((dvoid *)&(pdt->isPrimaryKey), sizeof(pdt->isPrimaryKey)		, SQLT_INT	,	"isPrimaryKey")	);

		for( int col=0; col<hosts.size(); col++ )
		{
			tuple<dvoid *, size_t, ub2, string> tpl = hosts[col];
			if (status = OCIDefineByPos(
				this->ociStmt,
				&(pdfn[col]),
				this->ociErr,
				(ub4)col+1,
				(dvoid *)get<0>(tpl),
				(sword)get<1>(tpl),
				(ub2)get<2>(tpl),
				(void *)0,
				(ub2 *)0,
				(ub2 *)0,
				(ub4)OCI_DEFAULT
			))
			{
				this->logErr(3, "OCIDefineByPos(", get<3>(tpl), ") is failed.");
				this->checkErr(status);
				goto finally;
			}

			/* 定義した各値ごとにスキップパラメータを指定する */
			if (status = OCIDefineArrayOfStruct(
				pdfn[col],
				this->ociErr,
				(ub4)sizeof(ColumnAttrs),
				(ub4)0,
				(ub4)0,
				(ub4)0
			)) {
				this->logErr(3, "OCIDefineArrayOfStruct(", get<3>(tpl), ") is failed.");
				this->checkErr(status);
				goto finally;
			}
		}

		//--------------------------------------------------------
		// SQL文を実行する
		//--------------------------------------------------------
		if (status = OCIStmtExecute(
			this->ociCtx,
			this->ociStmt,
			this->ociErr,
			(ub4)this->tblAttr->colCount,
			(ub4)0,
			(const OCISnapshot *)nullptr,
			(OCISnapshot *)nullptr,
			(ub4)OCI_DEFAULT
		)) {
			this->checkErr(status);
			goto finally;
		}

		//--------------------------------------------------------
		// データ長解析
		//--------------------------------------------------------
		for (int col = 0; col < colCnt; col++,pdt++)
		{
			// カラム名のトリム
			for (char *pc = (pdt->colName + (strlen(pdt->colName) - 1)); pc >= pdt->colName; pc--)
			{
				if (isspace(*pc))
				{
					*pc = '\0';
				}
				else {
					break;
				}
			}
			// 項目型の設定
			pdt->dataType = pdt->dataTypes[0];
			if (pdt->dataType == 'T')
			{
				pdt->colDateMask = (text *)TIMESTAMP_FMT;
			}
			else if (pdt->dataType == 'D')
			{
				pdt->colDateMask = (text *)DATE_FMT;
			}
//			else if (pdt->dataType == 'S')
//			{
//				pdt->fieldSize = (int)ceil(pdt->fieldSize*1.5);
//			}
			// レコード長累計
			lenSummary += pdt->fieldSize;
			// 終端文字列のサイズを追加
			lenSummary++;
		}

		//領域確保サイズ
		this->tblAttr->allocSize = (size_t)lenSummary * LOAD_UNIT;
		// 転送サイズ
		this->tblAttr->transferSize = (ub4)lenSummary * 1000;

		// ループ処理：デフォルト値生成
		for (int col = 0; col < colCnt; col++)
		{
			int colSize = (this->colAttr[col].fieldSize);
			char dataType = (this->colAttr[col].dataType);

			// char配列を格納領域にコピー
			string defVal = "";
			if (dataType == 'D')
			{
				defVal = "2017/01/01";
			}
			else if (dataType == 'T')
			{
				defVal = "2017/01/01 01:23:45.789";
			}
			else if (dataType == 'N') 
			{
				stringstream ss;
				ss << setw(colSize) << setfill('9') << "9";
				defVal = ss.str();
			}
			else 
			{
				stringstream ss;
				char *colName = this->colAttr[col].colName;
				size_t colNameLen = strlen(colName);
				for (int pos = 0; pos < colSize; pos++)
				{
					ss << colName[pos % colNameLen];
				}
				defVal = ss.str();
			}
			this->defaultsValue.push_back(defVal);
			//cout << "[" << defVal << "],size=" << (colSize) << endl;
		}
		ret = true;
finally:
		return ret;
	}

	/************************************************************************/
	/* ダイレクトパスロード準備												*/
	/************************************************************************/
	bool initDirectPathLoad()
	{
		ColumnAttrs	*pcol = this->colAttr;	// Column Define Attr Pointer
		OCIParam	*colDesc = nullptr;		// Column Descriptor
		sword		status = OCI_SUCCESS;

		// Create Direct-Path Context
		if (status = OCIHandleAlloc(
				this->ociEnv,
				(dvoid **)&(this->dpCtx),
				(const ub4)OCI_HTYPE_DIRPATH_CTX,
				(const size_t)0,
				(void **)0
		)) {
			this->logErr(1,"OCIHandleAlloc(OCI_HTYPE_DIRPATH_CTX) is failed.");
			this->checkErr(status);
			return false;
		}

		// Setting Direct-Path Context Attribute
		vector<tuple<void *, ub4, ub2, string>> dpctxAttrs;
		dpctxAttrs.push_back(make_tuple(this->tblAttr->tblName			, (ub4)strlen((char *)this->tblAttr->tblName)		, (ub2)OCI_ATTR_NAME						, "OCI_ATTR_NAME"));
		dpctxAttrs.push_back(make_tuple(this->tblAttr->tblOwner			, (ub4)strlen((char *)this->tblAttr->tblOwner)		, (ub2)OCI_ATTR_SCHEMA_NAME					, "OCI_ATTR_SCHEMA_NAME"));
		dpctxAttrs.push_back(make_tuple(&(this->tblAttr->input)			, (ub4)0											, (ub2)OCI_ATTR_DIRPATH_INPUT				, "OCI_ATTR_DIRPATH_INPUT"));
		dpctxAttrs.push_back(make_tuple(&(this->tblAttr->colCount)		, (ub4)0											, (ub2)OCI_ATTR_NUM_COLS					, "OCI_ATTR_NUM_COLS"));
		dpctxAttrs.push_back(make_tuple(&(this->tblAttr->noLog)			, (ub4)0											, (ub2)OCI_ATTR_DIRPATH_NOLOG				, "OCI_ATTR_DIRPATH_NOLOG"));
		dpctxAttrs.push_back(make_tuple(&(this->tblAttr->parallel)		, (ub4)0											, (ub2)OCI_ATTR_DIRPATH_PARALLEL			, "OCI_ATTR_DIRPATH_PARALLEL"));
		dpctxAttrs.push_back(make_tuple(&(this->tblAttr->indexSkip)		, (ub4)0											, (ub2)OCI_ATTR_DIRPATH_SKIPINDEX_METHOD	, "OCI_ATTR_DIRPATH_SKIPINDEX_METHOD"));
		dpctxAttrs.push_back(make_tuple(&(this->tblAttr->transferSize)	, (ub4)0											, (ub2)OCI_ATTR_BUF_SIZE					, "OCI_ATTR_BUF_SIZE"));

		// ループ：ダイレクトパスコンテキストの属性設定
		for (tuple<void *, ub4, ub2, string> &tpl : dpctxAttrs)
		{
			void *pattr = get<0>(tpl);
			ub4 attrSize = get<1>(tpl);
			ub4 attrKind = get<2>(tpl);
			const char *errMsg = get<3>(tpl).c_str();

			if (status = OCIAttrSet(
				this->dpCtx,
				OCI_HTYPE_DIRPATH_CTX,
				pattr,
				attrSize,
				attrKind,
				this->ociErr
			)) 
			{
				this->logErr(3, "OCIAttrSet",errMsg," is failed.");
				this->checkErr(status);
				return false;
			}
		}

		// Get the Column Parameter List
		if (status = OCIAttrGet(
			(const void *)this->dpCtx,
			(ub4)OCI_HTYPE_DIRPATH_CTX,
			(void *)&(this->colLstDesc),
			(ub4)0,
			(ub4)OCI_ATTR_LIST_COLUMNS,
			this->ociErr
		)) 
		{
			this->logErr(1, "OCIAttrGet(OCI_ATTR_LIST_COLUMNS) is failed.");
			this->checkErr(status);
			return false;
		}

		// Loop:Setting Column Describe
		for (ub4 col = 0; col < this->tblAttr->colCount; col++, pcol++)
		{
			stringstream idx;
			idx << col << ":" << (char *)pcol->colName;
			// Get the Column Parameter
			if (status = OCIParamGet(
				(const void *)this->colLstDesc,
				(ub4)OCI_DTYPE_PARAM,
				this->ociErr,
				(void **)&colDesc,
				(ub4)col + 1
			)) 
			{
				this->logErr(1, "OCIAttrGet(OCI_DTYPE_PARAM) is failed.");
				this->checkErr(status);
				return false;
			}
			// 列ポジションの設定
			pcol->colPos = col;

			// ダイレクトパス列配列の列属性設定
			vector<tuple<void *, ub4, ub4, string>> dpColAttrs;
			dpColAttrs.push_back(make_tuple(pcol->colName		, (ub4)strlen(pcol->colName), (ub4)OCI_ATTR_NAME		, "colName"));
			dpColAttrs.push_back(make_tuple(&(pcol->extendType)	, (ub4)0					, (ub4)OCI_ATTR_DATA_TYPE	, "extendType"));
			dpColAttrs.push_back(make_tuple(&(pcol->fieldSize)	, (ub4)0					, (ub4)OCI_ATTR_DATA_SIZE	, "fieldSize"));
			dpColAttrs.push_back(make_tuple(pcol->colDateMask	, (ub4)0					, (ub4)OCI_ATTR_DATEFORMAT	, "colDateMask"));
			//this->logInfo(3, "colName=[", pcol->colName, "]");

			// ループ処理：ダイレクトパス列配列の列属性設定
			for (tuple<void *, ub4, ub4, string> tpl : dpColAttrs)
			{
				void	*pvattr = get<0>(tpl);
				ub4		szattr = get<1>(tpl);
				ub4		kdattr = get<2>(tpl);
				string	nmattr = get<3>(tpl);

				if (!pvattr) break;

				if (status = OCIAttrSet(
					(void *)colDesc,
					(ub4)OCI_DTYPE_PARAM,
					(void *)pvattr,
					(ub4)szattr,
					(ub4)kdattr,
					this->ociErr
				)) 
				{
					this->logErr(3, "OCIAttrSet",nmattr," is failed.");
					this->checkErr(status);
					return false;
				}
			}// for loop terminate

			 // Descriptor解放
			if (status = OCIDescriptorFree(
				(void *)colDesc,
				(ub4)OCI_DTYPE_PARAM
			)) 
			{
				this->logErr(1, "OCIDescriptorFree is failed.");
				this->checkErr(status);
				return false;
			}
		}// for loop terminate

		 // ダイレクト・パス・ロード　インターフェースの準備
		this->logInfo(1, "OCIDirPathPrepare start.");
		if (status = OCIDirPathPrepare(
			this->dpCtx,
			this->ociCtx,
			this->ociErr
		)) 
		{
			this->logErr(1, "OCIDirPathPrepare is failed.");
			this->checkErr(status);
			return false;
		}

		// ダイレクトパス列配列生成
		this->logInfo(1, "OCIHandleAlloc(OCI_HTYPE_DIRPATH_COLUMN_ARRAY) start.");
		if (status = OCIHandleAlloc(
			this->dpCtx,
			(dvoid **)&(this->dpca),
			(ub4)OCI_HTYPE_DIRPATH_COLUMN_ARRAY,
			(size_t)0,
			(void **)0
		)) 
		{
			this->logErr(1, "OCIHandleAlloc(OCI_HTYPE_DIRPATH_COLUMN_ARRAY) is failed.");
			this->checkErr(status);
			return false;
		}

		// ダイレクトパスストリーム生成
		this->logInfo(1, "OCIHandleAlloc(OCI_HTYPE_DIRPATH_STREAM) start.");
		if (status = OCIHandleAlloc(
			this->dpCtx,
			(dvoid **)&(this->dpstr),
			(ub4)OCI_HTYPE_DIRPATH_STREAM,
			(size_t)0,
			(void **)0
		)) 
		{
			this->logErr(1, "OCIHandleAlloc(OCI_HTYPE_DIRPATH_STREAM) is failed.");
			this->checkErr(status);
			return false;
		}

		// ダイレクト列配列　行数取得
		if (status = OCIAttrGet(
			(const void *)this->dpca,
			(ub4)OCI_HTYPE_DIRPATH_COLUMN_ARRAY,
			(void *)&(this->dpRows),
			(ub4)0,
			(ub4)OCI_ATTR_NUM_ROWS,
			this->ociErr
		))
		{
			this->logErr(1, "OCIAttrGet(OCI_ATTR_NUM_ROWS) is failed.");
			this->checkErr(status);
			return false;
		}

		// ダイレクト列配列　列数取得
		if (status = OCIAttrGet(
			(const void *)this->dpca,
			(ub4)OCI_HTYPE_DIRPATH_COLUMN_ARRAY,
			(void *)&(this->dpCols),
			(ub4)0,
			(ub4)OCI_ATTR_NUM_COLS,
			this->ociErr
		))
		{
			this->logErr(1, "OCIAttrGet(OCI_ATTR_NUM_COLS) is failed.");
			this->checkErr(status);
			return false;
		}

		return true;
	}

	/************************************************************************/
	/* 入力レコード生成														*/
	/************************************************************************/
	bool generateInputRecord(int recCnt)
	{
		bool		ret		= false;
		size_t		colLen	= 0;
		time_t		tmt		= time(nullptr);
		time_t		dfTmt	= time(nullptr);
		struct tm	tmi;
		///fill_n(&tmi, sizeof(tmi), '\0');
		memset(&tmi, '\0', sizeof(tmi));

		//入力レコード配列の先頭ポインタを設定
		char		*ptcol = this->prcrds;
		// 入力レコード配列の領域クリア
		//fill_n(this->prcrds, this->tblAttr->allocSize, '\0');
		memset(this->prcrds, '\0', this->tblAttr->allocSize);

		// ループ処理：指定された行数分繰り返す
		for (int row = 0; row < recCnt; row++)
		{
			//cout << "row=" << row << endl;
			// ループ処理：1レコード当たりの項目数分繰り返す
			for (int col = 0; col < this->tblAttr->colCount; col++, ptcol++)
			{
				int		colSize = this->colAttr[col].fieldSize;
				char	dataType = this->colAttr[col].dataType;
				bool	isPrimaryKey = this->colAttr[col].isPrimaryKey;
				char	*ptrArg = nullptr;
				unsigned long defNum = 0;
				stringstream ss;
				string editVal;
				//cout << "[" << this->colAttr[col].colName << "]" << colSize << "," << dataType << endl;
				if (isPrimaryKey)
				{
					// プライマリキーの場合、一意となるよう値を生成
					switch (dataType)
					{
					case 'D':
						// Date型の場合
						//fill_n(&tmi, sizeof(tmi), '\0');
						memset(&tmi, '\0', sizeof(tmi));
						(void)localtime_s(&tmi, &tmt);
						ss << setw(4) << setfill('0') << tmi.tm_year + 1900 << '-'
							<< setw(2) << setfill('0') << (tmi.tm_mon + 1) << '-'
							<< setw(2) << setfill('0') << tmi.tm_mday << " "
							<< setw(2) << setfill('0') << tmi.tm_hour << ':'
							<< setw(2) << setfill('0') << tmi.tm_min << ':'
							<< setw(2) << setfill('0') << tmi.tm_sec;
						tmt = dfTmt + this->currentNo;
						break;
					case 'T':
						// TimeStamp型の場合
						//fill_n(&tmi, sizeof(tmi), '\0');
						memset(&tmi, '\0', sizeof(tmi));
						(void)localtime_s(&tmi, &tmt);
						defNum = 1000;
						ss << setw(4) << setfill('0') << tmi.tm_year + 1900 << '-'
							<< setw(2) << setfill('0') << (tmi.tm_mon + 1) << '-'
							<< setw(2) << setfill('0') << tmi.tm_mday << " "
							<< setw(2) << setfill('0') << tmi.tm_hour << ':'
							<< setw(2) << setfill('0') << tmi.tm_min << ':'
							<< setw(2) << setfill('0') << tmi.tm_sec << "."
							<< setw(3) << setfill('0') << (this->currentNo % defNum);
							;
						if ((this->currentNo % defNum) == 999) tmt += 1;
						break;
					case 'N':
						// 数値型の場合
						defNum = atol(this->defaultsValue[col].c_str());
						ss << setw(colSize - 1) << setfill('0') << (this->currentNo % defNum);
						break;
					default:
						// 文字列型の場合
						ss << setw(colSize - 1) << setfill('0') << hex << this->currentNo;
						break;
					}
					editVal = ss.str();
					ptrArg = (char *)(editVal.c_str());
					//cout << "editVal=[" << editVal << "]" <<  ",editVal.c_str()=[" << editVal.c_str() << "]" << ",ptrArg=[" << ptrArg << "]" << endl;
				}else {
					// 通常項目はデフォルト値
					ptrArg = (char *)this->defaultsValue[col].c_str();
				}
				colLen = strlen(ptrArg);
				//strcpy_s(ptcol, colLen, ptrArg);
				memcpy(ptcol, ptrArg, colLen);
				// 終端処理
				*(ptcol + colLen) = '\0';
				// 次の項目へ
				ptcol += colLen;
			}
			// カレント番号のインクリメント
			this->currentNo++;
		}
		return true;
	}

	/************************************************************************/
	/*  ダイレクトパスロード実行*/
	/************************************************************************/
	bool execDataLoad(int inputRcdCnt)
	{
		sword			status		 = OCI_SUCCESS;
		bool			done		 = false;
		bool			ret			 = true;
		bool			leftover	 = false;
		int				step		 = 0;
		int				dirPathRows	 = this->dpRows;
		int				dirPathCols	 = this->dpCols;
		int				okRows		 = 0;
		int				totalRows	 = 0;
		ub4				rowOffset	 = (ub4)0;
		ub4				curLoadCnt	 = (ub4)this->dpRows;
		ub4				clen		 = (ub4)0;
		ub1				cflg		 = (ub1)0;
		ub1				*cval		 = (ub1 *)this->prcrds;
		size_t			colLen		 = (size_t)0;
		stringstream	errMsg;

		// 作業ステップ：列挙
		enum steps{
			STEP_0_RESET,
			STEP_1_FIX_LOAD_CNT,
			STEP_2_SET_DATA_FIELD,
			STEP_3_CONVERT_STREAM,
			STEP_4_LOADING_STREAM,
			STEP_5_ADJUST_OFFSET,
			STEP_6_SUCCESS_COMPLETE,
			STEP_9_ERROR=9
		};

		// init step
		step = STEP_1_FIX_LOAD_CNT;

		// ループ処理：ダイレクトパスロードを指定行数分繰り返す
		while (!done)
		{
			switch (step)
			{
			case STEP_0_RESET:
				//----------------------------------------------------------------------
				// ダイレクトパス列配列＆ダイレクトパスストリームのリセット
				//----------------------------------------------------------------------
				// ダイレクトパス列配列のリセット
				if (status = OCIDirPathColArrayReset(
					this->dpca,
					this->ociErr
				)) {
					errMsg << "OCIDirPathColArrayReset - failed.";
					step = STEP_9_ERROR;
					break;
				}
				// ダイレクトパスストリームのリセット
				if (status = OCIDirPathStreamReset(
					this->dpstr,
					this->ociErr
				)) {
					errMsg << "OCIDirPathStreamReset - failed.";
					step = STEP_9_ERROR;
					break;
				}
				step = STEP_1_FIX_LOAD_CNT;
				break;
			case STEP_1_FIX_LOAD_CNT:
				//----------------------------------------------------------------------
				// 1回あたりのロード件数決定
				//----------------------------------------------------------------------
				if (totalRows + dirPathRows > inputRcdCnt)
				{
					// 累積ロード件数　＋　ダイレクトパス列配列行数＞入力ロード対象件数の場合、
					// 総ロード対象件数から累積ロード件数を差し引いた残存分件数をロードする。
					curLoadCnt = inputRcdCnt - totalRows;
				}else {
					// 上記以外の場合、ダイレクトパス列配列行数分ロードする。
					curLoadCnt = dirPathRows;
				}
				// 累積ロード件数の加算
				totalRows += curLoadCnt;
				if (curLoadCnt == 0)
				{
					// 今回ロード件数が0件の場合、終了
					step = STEP_6_SUCCESS_COMPLETE;
				}else {
					// 今回ロード件数＞0件の場合、次のステップへ
					step = STEP_2_SET_DATA_FIELD;
				}
				// ダイレクト列配列用のオフセットをクリア
				rowOffset = 0;
				break;
			case STEP_2_SET_DATA_FIELD:
				//----------------------------------------------------------------------
				// ダイレクトパス列配列　入力データフィールド設定
				//----------------------------------------------------------------------
				step = STEP_3_CONVERT_STREAM;
				for (ub4 row = 0; row < curLoadCnt; row++)
				{
					ColumnAttrs *pcol = this->colAttr;
					for (int col = 0; col < dirPathCols; col++)
					{
						colLen = strlen((char *)cval);
//						cout << "[" << row << "-" << col << "]:" << colp->columnName << ":colSz" << colp->columnSize << ":collen" << collen << ":[" << cval << "]" <<endl;
						clen = colLen;
						if (colLen == 0)
						{
							cflg = OCI_DIRPATH_COL_NULL;
						}
						else {
							cflg = OCI_DIRPATH_COL_COMPLETE;
						}
						if (pcol->fieldSize < colLen)
						{
							// テーブル項目定義上のカラムサイズより、実データのサイズが大きい場合、補正
							cout << "ColumnSize over. ROW=" << row << ",COLUMN_NAME=[" << pcol->colName << "],COLUMN_SIZE=" << pcol->fieldSize << ",DATA_SIZE=" << colLen;
							clen = pcol->fieldSize;
						}
						// ダイレクトパス列配列に入力データフィールド設定
						if (status = OCIDirPathColArrayEntrySet(
														this->dpca,
														this->ociErr,
														row,
														pcol->colPos,
														cval,
														clen,
														cflg
													)
						){
							errMsg << "OCIDirPathColArrayEntrySet - failed. ROW=" << row << ",COLUMN_NAME=[" << pcol->colName << "],COLUMN_SIZE=" << pcol->fieldSize << ",DATA_VALUE=[" << cval << "]";
							step = STEP_9_ERROR;
							break;
						}
						// 入力レコード配列のポインタを次の項目へ移動
						cval += colLen;
						cval++;
						pcol++;
					}
					// エラーが発生した場合、ループを終了
					if (step == STEP_9_ERROR) break;
				}
				break;
			case STEP_3_CONVERT_STREAM:
				//----------------------------------------------------------------------
				// ダイレクトパスストリームへの変換
				//----------------------------------------------------------------------
				leftover = false;
				// ダイレクトパス列配列をストリーム変換
				status = OCIDirPathColArrayToStream(
					this->dpca,
					this->dpCtx,
					this->dpstr,
					this->ociErr,
					curLoadCnt,
					rowOffset
				);
				if (status == OCI_SUCCESS) 
				{
					// ストリーム変換に成功
					step = STEP_4_LOADING_STREAM;
				}
				else if(status==OCI_NEED_DATA || status ==OCI_CONTINUE)
				{
					// 一部ストリーム変換未あり
					step = STEP_4_LOADING_STREAM;
					leftover = true;
				}
				else
				{
					errMsg << "OCIDirPathColArrayToStream - failed. ";
					step = STEP_9_ERROR;
				}
				break;
			case STEP_4_LOADING_STREAM:
				//----------------------------------------------------------------------
				// ダイレクトパスストリームのロード
				//----------------------------------------------------------------------
				status = OCIDirPathLoadStream(
					this->dpCtx,
					this->dpstr,
					this->ociErr
				);
				if (status == OCI_SUCCESS)
				{
					// 正常終了した場合はリセットへ
					step = STEP_0_RESET;
				}
				else if (status == OCI_NEED_DATA || status == OCI_CONTINUE || leftover)
				{
					// ストリーム変換未あり、またはロード未ありの場合、未ロード件数の特定へ
					step = STEP_5_ADJUST_OFFSET;
				}
				else
				{
					errMsg << "OCIDirPathLoadStream - failed. ";
					step = STEP_9_ERROR;
				}
				break;
			case STEP_5_ADJUST_OFFSET:
				//----------------------------------------------------------------------
				// ダイレクトパス列配列オフセットの調整
				//----------------------------------------------------------------------
				step = STEP_3_CONVERT_STREAM;
				if (status = OCIAttrGet(
										(const void *)this->dpca,
										(ub4)OCI_HTYPE_DIRPATH_COLUMN_ARRAY,
										(void *)&(okRows),
										(ub4)0,
										(ub4)OCI_ATTR_ROW_COUNT,
										this->ociErr
					)
				){
					errMsg << "OCIAttrGet(OCI_ATTR_ROW_COUNT) - failed. ";
					step = STEP_9_ERROR;
					break;
				}
				// ダイレクトパス列配列オフセットの調整
				rowOffset += okRows;
				if (status = OCIDirPathStreamReset(
					this->dpstr,
					this->ociErr
				)
					) {
					errMsg << "OCIDirPathStreamReset - failed. ";
					step = STEP_9_ERROR;
				}
				break;
			case STEP_6_SUCCESS_COMPLETE:
				//----------------------------------------------------------------------
				// ダイレクトパスロード処理完了
				//----------------------------------------------------------------------
				done = true;
				break;
			case STEP_9_ERROR:
			default:
				//----------------------------------------------------------------------
				// ダイレクトパスロード処理失敗
				//----------------------------------------------------------------------
				this->checkErr(status);
				this->logErr(1, errMsg.str().c_str());
				done = true;
				ret = false;
				break;
			}
		}
		return ret;
	}

public:
	/**************************************************************************
	 * スレッド初期化処理
	 **************************************************************************/
	boolean initialized()
	{
		sword	status = OCI_SUCCESS;

		this->logInfo(1, ">>>execute start ");
		//-------------------------------------------------
		// Oracle環境情報取得
		//-------------------------------------------------
		if (!this->getOracleEnvs())
		{
			this->logErr(1, "Oracle環境情報取得にてエラー発生");
			return false;
		}
		//-------------------------------------------------
		// OCI初期化
		//-------------------------------------------------
		if (!this->ociInit())
		{
			this->logErr(1, "OCI初期化にてエラー発生");
			return false;
		}
		//-------------------------------------------------
		// データベース接続
		//-------------------------------------------------
		if (!this->connectDB())
		{
			this->logErr(1, "データベース接続にてエラー発生");
			return false;
		}

		//-------------------------------------------------
		// Oracleロード対象テーブルメタ情報取得
		//-------------------------------------------------
		if (!this->getOracleMetaInfo())
		{
			this->logErr(1, "Oracleロード対象テーブルメタ情報取得");
			return false;
		}
		return true;
	}

	/************************************************************************/
	/* スレッド処理実行														*/
	/************************************************************************/
	bool execute()
	{
		sword			status = OCI_SUCCESS;
		unsigned int	recCnt = LOAD_UNIT;		// 既定件数
		unsigned int	totalCnt = 0;			// 累積件数
		stringstream	ss;

		ss << " [" << 
				setfill(' ') << setw(10) << this->startNo << "]->[" << 
				setfill(' ') << setw(10) << (this->startNo + this->count) - 1 << "]: count" << 
				setfill(' ') << setw(10) << this->count;
		this->logInfo(2, ">>> スレッド処理開始 ",ss.str().c_str());
		//-------------------------------------------------
		// OCI初期化
		//-------------------------------------------------
		if (!this->ociInit())
		{
			this->logErr(1, "OCI初期化にてエラー発生");
			return false;
		}
		//-------------------------------------------------
		// データベース接続
		//-------------------------------------------------
		if (!this->connectDB())
		{
			this->logErr(1, "データベース接続にてエラー発生");
			return false;
		}
		//-------------------------------------------------
		// 入力レコード配列を動的確保
		//-------------------------------------------------
		this->prcrds = new char[this->tblAttr->allocSize];
		if (this->prcrds == nullptr)
		{
			stringstream exceptMsg;
			exceptMsg << "メモリ動的確保に失敗.size=" << this->tblAttr->allocSize;
			this->logErr(1, exceptMsg.str().c_str());
			return false;
		}
		//fill_n(this->prcrds, this->tblAttr->allocSize, '\0');
		memset(this->prcrds, '\0', this->tblAttr->allocSize);

		//-------------------------------------------------
		// ダイレクトパスロード準備
		//-------------------------------------------------
		this->logInfo(1, "readyDirectPathLoad start");
		if (!this->initDirectPathLoad())
		{
			this->logErr(1, "ダイレクトパスロード準備にてエラー発生");
			return false;
		}

		// 既定件数に満たない場合は指定件数とする
		if (recCnt > this->count) recCnt = this->count;

		//-------------------------------------------------
		// ループ：ダイレクトパスロード処理
		//-------------------------------------------------
		while (true)
		{
			//-------------------------------------------------
			// 0件指定の場合は終了
			//-------------------------------------------------
			if (recCnt == 0) break;

			//-------------------------------------------------
			// 入力レコード生成
			//-------------------------------------------------
			if (!this->generateInputRecord(recCnt))
			{
				this->logErr(1, "ダイレクトパスロード準備にてエラー発生");
				return false;
			}

			//-------------------------------------------------
			// ダイレクトパスロードによるデータロード実行
			//-------------------------------------------------
			this->logInfo(1, "execDataLoad start");
			if (!this->execDataLoad(recCnt)) 
			{
				this->logErr(1, "ダイレクトパスロードによるデータロード実行にてエラー発生");
				return false;
			}

			//-------------------------------------------------
			// 累計件数カウント
			//-------------------------------------------------
			totalCnt += recCnt;
			stringstream cntVal;
			cntVal << totalCnt;
			this->logInfo(3, "total record cnt [", cntVal.str().c_str(), "] load success");

			//-------------------------------------------------
			// 件数が上限に満たない場合、次回処理予定のデータはないため終了
			//-------------------------------------------------
			if (recCnt < LOAD_UNIT) break;

			//-------------------------------------------------
			// 残存件数が既定件数未満の場合、残存件数を次回処理対象とする
			//-------------------------------------------------
			if ((this->count - totalCnt) < LOAD_UNIT) recCnt = (this->count - totalCnt);

		}// for loop terminate

		//-------------------------------------------------
		// ダイレクトパスロード処理のコミット
		//-------------------------------------------------
		this->logInfo(1, "OCIDirPathFinish start");
		if(status=OCIDirPathFinish(
								this->dpCtx,
								this->ociErr
		))
		{
			this->checkErr(status);
			this->logErr(1, "OCIDirPathFinish - failed.");
			return false;
		}
		this->logInfo(2, ">>> スレッド処理　正常終了 ", ss.str().c_str());
		return true;
	}

	/************************************************************************/
	/* スレッド処理呼出														*/
	/************************************************************************/
	void operator()()
	{
		bool result = this->execute();
		this->cleanupOci();
	}

};
// class終端


/************************************************************************/
/* 引数チェック＆メッセージ出力											*/
/************************************************************************/
void checkArg(char *arg, const char *argName)
{
	if (arg == nullptr || strlen(arg) == 0)
	{
		cout << "入力パラメータを正しく指定してください。[" << argName << "]" << endl;
		exit(1);
	}
}

/************************************************************************/
/* メイン関数															*/
/************************************************************************/
int main(int argc, char *argv[])
{
	char			*userName = nullptr;
	char			*tmp = nullptr;
	char			*passwd = nullptr;
	char			*svcName = nullptr;
	char			*tblName = nullptr;
	unsigned int	createCount = 0;
	unsigned int	countUnit = 0;
	unsigned int	countAmari = 0;
	unsigned int	parallel = 1;
	unsigned int	tmpStartNo = 1;
	int				result = 0;
	vector<thread>	threads;
	char *nextToken = nullptr;

	//-------------------------------------------------
	// 引数チェック
	//-------------------------------------------------
	if (argc < 5)
	{
		cout << "Usage: " << argv[0] << " user/passwd@service_name table_name create_count parallel" << endl;
		exit(1);
	}
	userName = strtok_s(argv[1], "/", &nextToken);
	checkArg(userName, "user");

	tmp = strtok_s(nullptr, "/", &nextToken);
	checkArg(tmp, "passwd");

	passwd = strtok_s(tmp, "@", &nextToken);
	checkArg(passwd, "passwd");

	svcName = strtok_s(nullptr, "@", &nextToken);
	checkArg(svcName, "service_name");

	tblName = argv[2];
	checkArg(tblName, "table_name");

	createCount = atoi(argv[3]);
	if (createCount <= 0)
	{
		cout << "入力パラメータ[create_count]には1以上の数値を指定してください" << endl;
		exit(1);
	}

	parallel = atoi(argv[4]);
	if (parallel <= 0 || parallel >=5)
	{
		cout << "入力パラメータ[parallel]には1～4の範囲内の数値を指定してください" << endl;
		exit(1);
	}

	//-------------------------------------------------
	// 1スレッド当たりの件数、端数を取得
	//-------------------------------------------------
	if (parallel == 1)
	{
		// 1スレッド当たりの件数は入力件数と同一
		countUnit = createCount;
	}
	else
	{
		countUnit = createCount / parallel;
		countAmari = createCount % parallel;
	}

	//-------------------------------------------------
	// 前準備
	//-------------------------------------------------
	DirectPathLoad *prDpl = new DirectPathLoad(
		userName,
		passwd,
		svcName,
		tblName
	);
	if (!prDpl->initialized())
	{
		cout << "ERROR: 致命的なエラーが発生しました。" << endl;
		exit(1);
	}

	cout << "*******************************************************************************" << endl;
	cout << "* LargeDataGenerator startup.													" << endl;
	cout << "*******************************************************************************" << endl;
	cout << "■インスタンス名：[" << prDpl->oraEnv->svcName << "]" << endl;
	cout << "■スキーマ名　　：[" << prDpl->oraEnv->userName << "]" << endl;
	cout << "■テーブル名　　：[" << prDpl->oraEnv->tblName << "]" << endl;
	cout << "■項目数　　　　：" << setfill(' ') << setw(10) << prDpl->tblAttr->colCount << endl;
	cout << "■総ロード件数　：" << setfill(' ') << setw(10) << createCount << endl;
	cout << "■並列スレッド数：" << setfill(' ') << setw(10) << parallel << endl;
	cout << "処理を続行しますか？(Y/N)>";

	string answer;
	cin >> answer;

	if (answer != "Y" && answer != "y")
	{
		cout << "*******************************************************************************" << endl;
		cout << "* LargeDataGenerator aborted.													" << endl;
		cout << "*******************************************************************************" << endl;
		exit(0);
	}

	//-------------------------------------------------
	// スレッド生成
	//-------------------------------------------------
	for (unsigned int threadNo = 1; threadNo <= parallel; threadNo++)
	{
		threads.push_back(
			thread(
				ref(
					*(new DirectPathLoad(
						prDpl->oraEnv,
						threadNo,
						tmpStartNo,
						countUnit + (threadNo < parallel ? 0 : countAmari),
						prDpl->tblAttr,
						prDpl->colAttr,
						prDpl->defaultsValue
					))
				)
			)
		);
		tmpStartNo += countUnit;
	}
	//-------------------------------------------------
	// スレッドのJOIN開始
	//-------------------------------------------------
	for (thread &th : threads)
	{
		th.join();
	}
	//-------------------------------------------------
	// リソースのクリーンアップ
	//-------------------------------------------------
	delete prDpl;

	cout << "*******************************************************************************" << endl;
	cout << "* LargeDataGenerator finishedd.												" << endl;
	cout << "*******************************************************************************" << endl;
	return 0;
}

