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
#define CONFIG_FTP_SERVER	"192.168.130.101"
#define CONFIG_FTP_PORT		2121
#define FILE_PATH			"\\esp32\\unit-test-app\\build\\unit-test-app.bin"
#define DIR_PATH			"\\esp32\\esp32-prj-uni"
#define USER			"user"
#define PASSWD			"password"
#define LOG_LEVEL       ESP_LOG_DEBUG

uint32_t binsize = 0;
uint32_t binread = 0;

// Estructura para almacenar la información del servidor FTP y la ruta del archivo
struct FTPFileInfo {
    char username[50];
    char password[50];
    char hostname[100];
    int port;
    char filepath[256]; // Ajusta el tamaño según tus necesidades
};

// Función para parsear la información del servidor FTP y la ruta del archivo
struct FTPFileInfo parseFTPFileInfo(const char *ftpString) {
    struct FTPFileInfo ftpFileInfo;
    // Inicializamos la estructura
    strcpy(ftpFileInfo.username, "");
    strcpy(ftpFileInfo.password, "");
    strcpy(ftpFileInfo.hostname, "");
    ftpFileInfo.port = 21;  // Puerto FTP por defecto
    strcpy(ftpFileInfo.filepath, "");

    // Usamos sscanf para parsear los campos
	// Si no tenemos el caracter @ en la cadena, no tenemos usuario ni contraseña
	char* serverPointer = NULL;
	serverPointer = strchr(ftpString, '@');
	bool hasUserPass = (strchr(ftpString, '@') != NULL);
	bool hasHead = (strstr(ftpString, "ftp://") != NULL);
	char* auxPointer = hasHead ? (serverPointer!=NULL ? serverPointer+1:((char*)strstr(ftpString, "ftp://")) + 7) : (char*)ftpString; //Si tenemos la cabecera poscionamos el puntero a partir de ftp:// para buscar ':' que indicarian puerto
	bool hasPort = (strchr(auxPointer, ':') != NULL) ? true : false;

	DEBUG_TRACE_I(_EXPR_, _MODULE_, "Init: %p, auzPointer: %p->%s", (void*)ftpString, (void*)auxPointer, auxPointer);
	

	if(hasUserPass && hasPort){
		DEBUG_TRACE_I(_EXPR_, _MODULE_, "[%s]: %d", __func__, __LINE__);
		sscanf(ftpString, "ftp://%49[^:]:%49[^@]@%99[^:]:%d/%255[^\n]", ftpFileInfo.username, ftpFileInfo.password, ftpFileInfo.hostname, &ftpFileInfo.port, ftpFileInfo.filepath);
	}
	else{
		if(hasUserPass){
			if(hasPort){
				DEBUG_TRACE_I(_EXPR_, _MODULE_, "[%s]: %d", __func__, __LINE__);
				sscanf(ftpString, "ftp://%49[^:]:%49[^@]@%99[^:]:%d/%255[^\n]", ftpFileInfo.username, ftpFileInfo.password, ftpFileInfo.hostname, &ftpFileInfo.port, ftpFileInfo.filepath);
			}
			else{
				DEBUG_TRACE_I(_EXPR_, _MODULE_, "[%s]: %d", __func__, __LINE__);
				sscanf(ftpString, "ftp://%49[^:]:%49[^@]@%99[^/]/%255[^\n]", ftpFileInfo.username, ftpFileInfo.password, ftpFileInfo.hostname, ftpFileInfo.filepath);
			}
		}
		else{
			if(hasPort){
				DEBUG_TRACE_I(_EXPR_, _MODULE_, "[%s]: %d", __func__, __LINE__);
				sscanf(ftpString, "ftp://%99[^:]:%d/%255[^\n]", ftpFileInfo.hostname, &ftpFileInfo.port, ftpFileInfo.filepath);
			}
			else{
				DEBUG_TRACE_I(_EXPR_, _MODULE_, "[%s]: %d", __func__, __LINE__);
				sscanf(ftpString, "ftp://%99[^/]/%255[^\n]", ftpFileInfo.hostname, ftpFileInfo.filepath);
			}
		}
	}
    return ftpFileInfo;
}


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
TEST_CASE("Parseo Url completa", "[TEST_FtpCli]"){
	const char *ftpString = "ftp://usuario:contrasena@ftp.ejemplo.com:2121/archivo/directorio/archivo.txt";
    
    // Llamamos a la función para obtener la información del servidor FTP y la ruta del archivo
    struct FTPFileInfo ftpFileInfo = parseFTPFileInfo(ftpString);

    // Imprimimos la información parseada
    DEBUG_TRACE_I(_EXPR_, _MODULE_, "Usuario: %s", ftpFileInfo.username);
    DEBUG_TRACE_I(_EXPR_, _MODULE_, "Contraseña: %s", ftpFileInfo.password);
    DEBUG_TRACE_I(_EXPR_, _MODULE_, "Hostname: %s", ftpFileInfo.hostname);
    DEBUG_TRACE_I(_EXPR_, _MODULE_, "Puerto: %d", ftpFileInfo.port);
    DEBUG_TRACE_I(_EXPR_, _MODULE_, "Ruta del archivo: %s", ftpFileInfo.filepath);

	TEST_ASSERT_EQUAL_STRING("usuario", ftpFileInfo.username);
	TEST_ASSERT_EQUAL_STRING("contrasena", ftpFileInfo.password);
	TEST_ASSERT_EQUAL_STRING("ftp.ejemplo.com", ftpFileInfo.hostname);
	TEST_ASSERT_EQUAL_INT(2121, ftpFileInfo.port);
	TEST_ASSERT_EQUAL_STRING("archivo/directorio/archivo.txt", ftpFileInfo.filepath);
}

//------------------------------------------------------------------------------------
TEST_CASE("Parseo url_port_path", "[TEST_FtpCli]"){
	const char *ftpString = "ftp://ftp.ejemplo.com:2121/archivo/directorio/archivo.txt";
    
    // Llamamos a la función para obtener la información del servidor FTP y la ruta del archivo
    struct FTPFileInfo ftpFileInfo = parseFTPFileInfo(ftpString);

    // Imprimimos la información parseada
    DEBUG_TRACE_I(_EXPR_, _MODULE_, "Usuario: %s", ftpFileInfo.username);
    DEBUG_TRACE_I(_EXPR_, _MODULE_, "Contraseña: %s", ftpFileInfo.password);
    DEBUG_TRACE_I(_EXPR_, _MODULE_, "Hostname: %s", ftpFileInfo.hostname);
    DEBUG_TRACE_I(_EXPR_, _MODULE_, "Puerto: %d", ftpFileInfo.port);
    DEBUG_TRACE_I(_EXPR_, _MODULE_, "Ruta del archivo: %s", ftpFileInfo.filepath);

	TEST_ASSERT_EQUAL_STRING("", ftpFileInfo.username);
	TEST_ASSERT_EQUAL_STRING("", ftpFileInfo.password);
	TEST_ASSERT_EQUAL_STRING("ftp.ejemplo.com", ftpFileInfo.hostname);
	TEST_ASSERT_EQUAL_INT(2121, ftpFileInfo.port);
	TEST_ASSERT_EQUAL_STRING("archivo/directorio/archivo.txt", ftpFileInfo.filepath);
}

//------------------------------------------------------------------------------------
TEST_CASE("Parseo url_path", "[TEST_FtpCli]"){
	const char *ftpString = "ftp://ftp.ejemplo.com/archivo/directorio/archivo.txt";
    
    // Llamamos a la función para obtener la información del servidor FTP y la ruta del archivo
    struct FTPFileInfo ftpFileInfo = parseFTPFileInfo(ftpString);

    // Imprimimos la información parseada
    DEBUG_TRACE_I(_EXPR_, _MODULE_, "Usuario: %s", ftpFileInfo.username);
    DEBUG_TRACE_I(_EXPR_, _MODULE_, "Contraseña: %s", ftpFileInfo.password);
    DEBUG_TRACE_I(_EXPR_, _MODULE_, "Hostname: %s", ftpFileInfo.hostname);
    DEBUG_TRACE_I(_EXPR_, _MODULE_, "Puerto: %d", ftpFileInfo.port);
    DEBUG_TRACE_I(_EXPR_, _MODULE_, "Ruta del archivo: %s", ftpFileInfo.filepath);

	TEST_ASSERT_EQUAL_STRING("", ftpFileInfo.username);
	TEST_ASSERT_EQUAL_STRING("", ftpFileInfo.password);
	TEST_ASSERT_EQUAL_STRING("ftp.ejemplo.com", ftpFileInfo.hostname);
	TEST_ASSERT_EQUAL_INT(21, ftpFileInfo.port);
	TEST_ASSERT_EQUAL_STRING("archivo/directorio/archivo.txt", ftpFileInfo.filepath);
}

//------------------------------------------------------------------------------------
TEST_CASE("Parseo url_path ejemplo", "[TEST_FtpCli]"){
	const char *ftpString = "ftp://demo:password@test.rebex.net/pub/example/readme.txt";
    
    // Llamamos a la función para obtener la información del servidor FTP y la ruta del archivo
    struct FTPFileInfo ftpFileInfo = parseFTPFileInfo(ftpString);

    // Imprimimos la información parseada
    DEBUG_TRACE_I(_EXPR_, _MODULE_, "Usuario: %s", ftpFileInfo.username);
    DEBUG_TRACE_I(_EXPR_, _MODULE_, "Contraseña: %s", ftpFileInfo.password);
    DEBUG_TRACE_I(_EXPR_, _MODULE_, "Hostname: %s", ftpFileInfo.hostname);
    DEBUG_TRACE_I(_EXPR_, _MODULE_, "Puerto: %d", ftpFileInfo.port);
    DEBUG_TRACE_I(_EXPR_, _MODULE_, "Ruta del archivo: %s", ftpFileInfo.filepath);

	TEST_ASSERT_EQUAL_STRING("demo", ftpFileInfo.username);
	TEST_ASSERT_EQUAL_STRING("password", ftpFileInfo.password);
	TEST_ASSERT_EQUAL_STRING("test.rebex.net", ftpFileInfo.hostname);
	TEST_ASSERT_EQUAL_INT(21, ftpFileInfo.port);
	TEST_ASSERT_EQUAL_STRING("pub/example/readme.txt", ftpFileInfo.filepath);
}

//------------------------------------------------------------------------------------
TEST_CASE("Conectar red wifi.", "[TEST_FtpCli]"){
	test_wifi_connect();
	TEST_ASSERT_TRUE(WifiInterface::hasIp());
}


//------------------------------------------------------------------------------------
TEST_CASE("Inicializacion FtpCli.", "[TEST_FtpCli]"){
	ftpClient = getFtpClient(LOG_LEVEL);
	TEST_ASSERT_NOT_NULL(ftpClient);
}

//------------------------------------------------------------------------------------
TEST_CASE("Set callback function.", "[TEST_FtpCli]"){


	cbOpt.cbFunc = NULL;
	cbOpt.cbArg = NULL;
	cbOpt.bytesXferred = 0;
	cbOpt.idleTime = 0;
	cbOpt.cbFunc2 = callback(&ftpClientCb2);;//ftpClientCb2;
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
	if(ftpClient->ftpClientGetFileSize(FILE_PATH, &size, FTP_CLIENT_BINARY, ftpClientNetBuf)!=1){
		DEBUG_TRACE_E(_EXPR_, _MODULE_, "Error al obtener el SIZE del archivo");
	}
	else{
		binsize = size;
		DEBUG_TRACE_I(_EXPR_, _MODULE_, "size=%d", size);
		DEBUG_TRACE_E(_EXPR_, _MODULE_, "FILE_PATH=%s", FILE_PATH);
	}
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

