/*
 * test_fetpCli.cpp
 *
 *	Test unitario para la clase FtpClient
 *  Para personalizar el test, modificar las siguientes constantes:
 * - SSID	: red wifi a la que conectarse
 * - PASS	: password de la red wifi
 * - CONFIG_FTP_SERVER	: IP del servidor FTP
 * - CONFIG_FTP_PORT	: Puerto del servidor FTP
 * - FILE_PATH			: Path del archivo a descargar
 * - DIR_PATH			: Path del directorio a listar
 * - USER				: USER para hacer login en el servidor FTP
 * - PASSWD				: PASSWD para hacer login en el servidor FTP
 */

//------------------------------------------------------------------------------------
//-- TEST HEADERS --------------------------------------------------------------------
//------------------------------------------------------------------------------------
#include "unity.h"
#include "mbed.h"
#include "AppConfig.h"
#include "WifiInterface.h"
#include "FtpClient.h"

//------------------------------------------------------------------------------------
//-- SPECIFIC COMPONENTS FOR TESTING -------------------------------------------------
//------------------------------------------------------------------------------------

static const char* _MODULE_ = "[TEST_FtpCli]....";
#define _EXPR_	(true)

//------------------------------------------------------------------------------------
//-- REQUIRED HEADERS & COMPONENTS FOR TESTING ---------------------------------------
//------------------------------------------------------------------------------------
static const char* SSID = "WIFI-Invitado-Alc";
static const char* PASS = "A1c_Invitado@2019";
#define CONFIG_FTP_SERVER	"192.168.130.103"
#define CONFIG_FTP_PORT		2121
#define FILE_PATH			"software\\esp32\\unit-test-app\\build\\unit-test-app.bin"
#define DIR_PATH			"software\\esp32\\esp32-prj-uni"
#define USER			"user"
#define PASSWD			"password"

uint32_t binsize = 0;
uint32_t binread = 0;


FtpClient* ftpClient = NULL;
NetBuf_t *ftpClientNetBuf = NULL;

// Callback function que recibe el numero de datos recibidos
int ftpClientCb(NetBuf_t* nControl, uint32_t xfered, void* arg){
	DEBUG_TRACE_I(_EXPR_, _MODULE_, "xfered=%d ", xfered);
	return 1;
}

// Callback function que recibe el buffer de datos
int ftpClientCb2(NetBuf_t* nControl, uint32_t xfered, char* buf){
	binread += xfered;
	DEBUG_TRACE_I(_EXPR_,_MODULE_, "Downloaded %d/%d bytes", binread, binsize);
	return 1;
}

FtpClientCallbackOptions_t cbOpt;

//------------------------------------------------------------------------------------
//-- TEST FUNCTIONS ------------------------------------------------------------------
//------------------------------------------------------------------------------------
void test_wifi_connect(){
	DEBUG_TRACE_I(_EXPR_, _MODULE_, "Inicializamos nvs");
	nvs_flash_init();
	esp_log_level_set(_MODULE_, ESP_LOG_DEBUG); 

	wifi_mode_t wmode = WIFI_MODE_NULL;
	std::vector<WifiInterface::Sta_t*> *_sta_list;
	_sta_list =  new std::vector<WifiInterface::Sta_t*>();
	MBED_ASSERT(_sta_list);

	/*************** WifiInterface ***************/
	if(!WifiInterface::isInitialized()){
    	//Arranco siempre en modo APSTA para buscar la BACK DOOR
    	wmode = WIFI_MODE_STA;
		DEBUG_TRACE_I(_EXPR_, _MODULE_, "Arrancamos red wifi para conectarnos");
		WifiInterface::wifiInit(wmode,SSID, PASS);
	}

	if(strlen(SSID) > 0){
		DEBUG_TRACE_I(_EXPR_, _MODULE_,"Cargamos info wifi usuario!");
		WifiInterface::Sta_t *_sta_user = new WifiInterface::Sta_t();
		MBED_ASSERT(_sta_user);
		strncpy(_sta_user->name,"sta_user",strlen("sta_user")+1);
		strncpy(_sta_user->sta_essid,SSID,32);
		strncpy(_sta_user->sta_passwd,PASS,64);
		DEBUG_TRACE_D(_EXPR_, _MODULE_, "name:%s essid=%s, pass=%s",_sta_user->name,_sta_user->sta_essid,_sta_user->sta_passwd);
		_sta_list->push_back(_sta_user);
	}
	else{
		DEBUG_TRACE_E(_EXPR_, _MODULE_,"No hay info wifi usuario!");
	}


	DEBUG_TRACE_D(_EXPR_, _MODULE_, "Arrancando WifiInterface essid=%s, pass=%s", SSID, PASS);
	WifiInterface::startSTA(_sta_list);
	while(!WifiInterface::isStarted()){
		Thread::wait(100);
		DEBUG_TRACE_I(_EXPR_, _MODULE_, "Esperando started...");
	}
	DEBUG_TRACE_D(_EXPR_, _MODULE_,"WifiInterface STA OK!");

	//Esperamos a tener ip
	while(!WifiInterface::hasIp()){
		Thread::wait(1000);
		DEBUG_TRACE_I(_EXPR_, _MODULE_, "Esperando IP...");
	}
}

//------------------------------------------------------------------------------------
//-- TEST CASES ----------------------------------------------------------------------
//------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------
TEST_CASE("Conectar red wifi.", "[TEST_FtpCli]"){
	test_wifi_connect();
	TEST_ASSERT_TRUE(WifiInterface::hasIp());
}


//------------------------------------------------------------------------------------
TEST_CASE("Inicializacion FtpCli.", "[TEST_FtpCli]"){
	ftpClient = getFtpClient();
	TEST_ASSERT_NOT_NULL(ftpClient);
}

//------------------------------------------------------------------------------------
TEST_CASE("Set callback function.", "[TEST_FtpCli]"){
	cbOpt.cbFunc = NULL;
	cbOpt.cbArg = NULL;
	cbOpt.bytesXferred = 0;
	cbOpt.idleTime = 0;
	cbOpt.cbFunc2 = ftpClientCb2;
	int res = ftpClient->ftpClientSetCallback(&cbOpt, ftpClientNetBuf);
	DEBUG_TRACE_I(_EXPR_, _MODULE_, "res=%d", res);
	TEST_ASSERT_EQUAL_INT(1, res);
}

//------------------------------------------------------------------------------------
TEST_CASE("Connect to server.", "[TEST_FtpCli]"){
	// Open FTP server
	DEBUG_TRACE_I(_EXPR_, _MODULE_, "ftp server:%s", CONFIG_FTP_SERVER);

	int connect = ftpClient->ftpClientConnect(CONFIG_FTP_SERVER, CONFIG_FTP_PORT, &ftpClientNetBuf);
	DEBUG_TRACE_I(_EXPR_, _MODULE_, "connect=%d", connect);
	TEST_ASSERT_EQUAL_INT(1, connect);
}

//------------------------------------------------------------------------------------
TEST_CASE("Login to server.", "[TEST_FtpCli]"){
	int login = ftpClient->ftpClientLogin(USER, PASSWD, ftpClientNetBuf);
	DEBUG_TRACE_I(_EXPR_, _MODULE_, "login=%d", login);
}

//------------------------------------------------------------------------------------
TEST_CASE("Pwd.", "[TEST_FtpCli]"){
	char pwdPath[128];
	int res = ftpClient->ftpClientPwd(pwdPath, 5, ftpClientNetBuf);
	DEBUG_TRACE_I(_EXPR_, _MODULE_, "pwd=%s", pwdPath);
	TEST_ASSERT_EQUAL_INT(1, res);
}

//------------------------------------------------------------------------------------
TEST_CASE("List of file name in the dir.", "[TEST_FtpCli]"){
	char dirPath[512];
	int res = ftpClient->ftpClientNlst(dirPath, DIR_PATH, ftpClientNetBuf);
	DEBUG_TRACE_I(_EXPR_, _MODULE_, "dir=%s", dirPath);
	TEST_ASSERT_EQUAL_INT(1, res);
}

//------------------------------------------------------------------------------------
TEST_CASE("Dir UP.", "[TEST_FtpCli]"){
	int res = ftpClient->ftpClientChangeDirUp(ftpClientNetBuf);
	TEST_ASSERT_EQUAL_INT(1, res);
}

//------------------------------------------------------------------------------------
TEST_CASE("Change dir.", "[TEST_FtpCli]"){
	int res = ftpClient->ftpClientChangeDir(DIR_PATH, ftpClientNetBuf);
	TEST_ASSERT_EQUAL_INT(1, res);
}

//------------------------------------------------------------------------------------
//Muestr el contenido del directorio remoto
TEST_CASE("Get file size.", "[TEST_FtpCli]"){
	unsigned int size = 0;
	ftpClient->ftpClientGetFileSize(FILE_PATH, &size, FTP_CLIENT_BINARY, ftpClientNetBuf);
	binsize = size;
	DEBUG_TRACE_I(_EXPR_, _MODULE_, "size=%d", size);
}

//------------------------------------------------------------------------------------
//Descarga un archivo
TEST_CASE("Get file.", "[TEST_FtpCli]"){
	char dirPath[512] = {0};
	binread = 0;
	int res = ftpClient->ftpClientGet(dirPath, FILE_PATH, FTP_CLIENT_BINARY, ftpClientNetBuf);
	TEST_ASSERT_EQUAL_INT(1, res);
}


//------------------------------------------------------------------------------------
TEST_CASE("Disconnect from server.", "[TEST_FtpCli]"){
	ftpClient->ftpClientQuit(ftpClientNetBuf);
}

