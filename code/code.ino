// *******************************************************************
// *     CÓDIGO LIVRE DE PROCESSAMENTO DO DISPOSITIVO "OPENFLOW"     *
// ******************************************************************* 
//
//   Autores: Germano Adelino Gallo e Aderivaldo Cabral Dias Filho.
//   Pode ser copiado, editado e distribuído, desde que mantida referência aos autores.
//
//   Código utilizado na dissertação de mestrado de Germano Adelino Gallo junto a Universidade de Brasília - UnB.
//
// *************************************
// *      HISTÓRICO DE VERSÕES         *
// *************************************
//
// - 1.00: Definição de pinos;
// - 1.10: Introdução de bibliotecas para comunicação I2c e SPI;
// - 1.20: Teste de comunicação com RTC;
// - 1.21: Definição dos nomes dos arquivos de registro baseado em informações do RTC;
// - 1.30: Criação de rotinas de som (módulo buzzer);
// - 1.40: Teste da célula de carga (sem successo na comunicação);
// - 1.41: Obtenção dados da célula de carga através dos pinos analógicos A0 e A1;
// - 1.50: Criação de rotinas de calibração;
// - 1.60: Comunicação dos dispositivos e teste de calibração;
// - 1.80: Primeira versão operacional para avaliação das curvas;
// - 1.90: Inserida rotina "SLEEP" para economia de energia;
// - 2.00: Ajuste de tara para 200g e tempo de loop para 50 milissegundos;
// - 2.01: Corrigido "bugs" no modo de economia de energia;
// - 2.02: Reduzido "delay" para início de coleta com tempo de 3 segundos;
// - 2.03: Introduzida rotina para retirar números negativos da coleta;
// - 2.04: Modificados separadores da variável data-tempo para compatibilidade com o formato POSIX do R;
// - 2.05: Introduzido código para controle de LED;
// - 2.06: Ajustado "delay" para 10 registros por segundo (10 Hz);
// - 2.07: Ativação automática do modo de calibração se não encontrado arquivo config.ini;
// - 2.08: Adicionados "zeros" na criação do nome do arquivo; 
// - 2.10: Organização de nomes de pastas e arquivos:
//          - config.ini dentro de /OPENFLOW;
//          - dados coletados dentro de /OPENFLOW/DADOS;
//          - DD-MM-AA como diretório e HH-MM-SS como nome do arquivo, mantendo formato CSV.
// - 2.11: Criada rotina para aviso na falha dos módulos:
//          - Dois bipes: falha ao iniciar cartão SD;
//          - Três bipes: falha ao iniciar módulo RTC;
//          - Quatro bipes: falha ao iniciar módulo HX711.
// - 2.12: Implementada ativação do modo de calibração ao ligar e desligar o sistema 5 vezes no intervalo de 10 segundos.
// - 2.13: Checagem dos limites da tara e aviso da necessidade de nova calibração.
// - 2.14: Compensação da tara durante início da coleta.
// - 2.15: Mudado padrão de calibração para 300 mL, conforme recomendação da ICS. Fluxo de calibração necessário: 15 mL/s.
// - 2.16: Adicionada função de ajuste de densidade do líquido. 
// - 2.17: Se o peso instantâneo oscilar negativamente durante a coleta, manterá o valor anterior, reduzindo artefatos. 

// Necessária inclusão das bibliotecas HX711 e SdFat. Caminho: Menu "Sketch", "Incluir Biblioteca", "Gerenciar Bibliotecas...".
// Buscar e instalar a biblioteca "Queuetue HX711 Arduino Library". Biblioteca IniFile disponível em código aberto.

// Atribuição de pinos:

const byte pino_SDA_HX7 = A0; // Pino SDA.
const byte pino_SCL_HX7 = A1; // Pino SCL.
const int pino_buzzer = 8; // Pino de som.
const int Pino_CS = 4; // Pino do Cartão SD.
const int Pino_LED = 7; // Pino do LED.

// Variáveis de ambiente:

float Densidade = 1015; // Densidade da urina. 
float Peso_Calibracao = 300.0; // Valor do peso de calibração. Pode ser modificado. ICS recomenda 300 mL. 
float NumeroAfericoes = 10.0; // Número de médias de entrada de massa para cada registro de aferição na calibração.  
float Percent_Recipiente = 10; // Percentual de tolerância entre o peso real do frasco e o aferido, sugerindo nova calibração.
float Delay = 80; // Ajusta o "delay" para adequação da coleta (valor encontrado para 10 Hz).
int Freq_Som = 392; // Código da frequência do som. 
long RawZero = 0L;
long RawCalibracao = 0L;
long RawRecipiente = 0L;
long RawTara = 0L;
String nomeArquivo;
float tempo = 0;
unsigned long sleepTime;
String horario;
String zerodia;
String zeromes;
String zerohora;
String zeromin;
String zeroseg;
unsigned long contadorLED = 0;
int OnOff = 0;
int segundos = 0;
long RawAvisotara;
float coletamomento;
float tarainst;

// Bibliotecas:

#include <Q2HX711.h> // Biblioteca do módulo HX711, responsável por amplificar o sinal da célula de carga.
#include <RTClib.h> // Biblioteca do módulo RTC.
#include <Wire.h> // Biblioteca para comunicação I2c.
#include <SdFat.h> // Biblioteca para comunicação com módulo SD.
#include <SPI.h> // Biblioteca de comunicação SPI.
#include <IniFile.h> // Biblioteca para gerenciamento de arquivos INI (modificada).
#include <Sleep_n0m1.h>
#include <EEPROM.h>

// Definições:

Sleep sleep;
DateTime agora;
#pragma execution_character_set("utf-8")
Q2HX711 hx711(pino_SDA_HX7, pino_SCL_HX7);
File arquivo;
RTC_DS1307 rtc;
SdFat sd;


// Início da programação:


void setup() {

  rtc.begin();

  agora = rtc.now();
  
  OnOff = EEPROM.read(0);
  segundos = ((agora.hour()*3600) + (agora.minute()*60) + (agora.second())) - ((EEPROM.read(1)*3600) + (EEPROM.read(2)*60) + (EEPROM.read(3)));
  EEPROM.write(1, agora.hour());
  EEPROM.write(2, agora.minute());
  EEPROM.write(3, agora.second());
  
  if (segundos < 5) {
    OnOff++;
  } else {
    OnOff = 0;
  }

  EEPROM.write(0, OnOff);

  sleepTime = 604800000;
    
  pinMode(pino_buzzer,OUTPUT); // Configura pino sonoro no modo saída.
  pinMode(Pino_CS, OUTPUT); // Configura pino do cartão SD como saída. 
  pinMode(Pino_LED, OUTPUT);  // Configura pino do LED como saída.
  
  
// Definição do arquivo config.ini:


  const size_t bufferLen = 80;
  char buffer[bufferLen];
  const char *filename = "/OPENFLOW/config.ini";


// Iniciar processos:


  digitalWrite(Pino_LED, HIGH); // Liga LED;

  Serial.begin(9600); // Inicia transmissão no monitor serial.

  SPI.begin(); // Inicia protocolo de transmissão SPI.
  
  if (!sd.begin(Pino_CS)) { // Emite dois bipes se módulo SD não iniciar.

    som_200();
    som_200();
    while(1);   
  }
  
  if (!rtc.begin()) { // Emite três bipes se módulo RTC não iniciar.

    som_200();
    som_200();
    som_200();
    while(1);  
  }

   if (hx711.readyToSend() == 1) { // Emite quatro bipes se módulo HX711 não iniciar.
    
    som_200();
    som_200();
    som_200();
    som_200();
    while(1);
  }

  Wire.begin(); // Inicia transmissão I2c.


  // Coleta inicial de configurações e rotinas do cartão SD:


  IniFile ini(filename);

  criarNomeArquivoDiretorio(); // Inicia rotina para criação do nome do arquivo e pasta.

  criarDiretorioSistema(); // Criar a pasta do sistema caso não seja encontrada. 

  if (OnOff >= 5) { // Entra no modo de calibração se ligado e desligado rapidamente por 5 vezes.
    OnOff = 0;
    EEPROM.write(0, OnOff);
    calibracao();
    while(1);
  }

  if (!ini.open()) { // Procura o arquivo config.ini, se não encontrar entra no modo calibração.
    calibracao();
    while(1);
  }

  ini.getValue("CONFIGURACAO", "Valor", buffer, bufferLen);
  RawCalibracao = atol(buffer);

  ini.getValue("CONFIGURACAO", "Tara", buffer, bufferLen);
  RawTara = atol(buffer);

  ini.getValue("CONFIGURACAO", "Zero", buffer, bufferLen);
  RawZero = atol(buffer);


// Aviso de necessidade de nova calibração:


  RawAvisotara = hx711.read();
  
  float Razao_1 = ((RawTara - RawZero)*Peso_Calibracao)/(RawCalibracao-RawTara);
  float Razao_2 = ((RawAvisotara - RawZero)*Peso_Calibracao)/(RawCalibracao-RawTara);
  
  if (Razao_2 > Razao_1*(Percent_Recipiente/100 + 1) || Razao_2 < Razao_1*(1-Percent_Recipiente/100) ) {  
    
    digitalWrite(Pino_LED, LOW); // Desliga LED;
    delay(100);
    led_pisca();
    led_pisca();
    led_pisca();
    digitalWrite(Pino_LED, HIGH); // Liga LED;
  }

  delay(3000); // Aguarda 3 segundos antes de iniciar a coleta de dados.

  tone(pino_buzzer,Freq_Som, 2000); 
  delay(2000); 
}


// Loop de coleta de dados:


void loop() {   

  long ColetaRaw = 0;
  ColetaRaw = hx711.read();
    
  agora = rtc.now();

  if (agora.hour() >=0 && agora.hour() < 10) { zerohora = "0"; } else {zerohora = "";};
  if (agora.minute() >=0 && agora.minute() < 10) { zeromin = "0"; } else {zeromin = "";};
  if (agora.second() >=0 && agora.second() < 10) { zeroseg = "0"; } else {zeroseg = "";};

  horario = zerohora + String(agora.hour()) + ':' + zeromin + String(agora.minute()) + ':' + zeroseg + String(agora.second());
  
  float Razao_1 = (float) (ColetaRaw-RawTara);
  float Razao_2 = (float) (RawCalibracao-RawTara);
  float Razao = Razao_1/Razao_2;
  float PesoInst = Peso_Calibracao*Razao - tarainst; // Compensa pequena variação inicial na coleta de dados. 

  if (PesoInst < 0) { // Não aceita peso negativo na coleta de dados. 
      PesoInst = 0;
  }

  if (millis() > contadorLED+1000) { // Configura LED para piscar a cada segundo.
    contadorLED = millis();
    digitalWrite(Pino_LED, !digitalRead(Pino_LED));
  }
    
  if (coletamomento > PesoInst) {PesoInst = coletamomento;} // Evita registros instantâneos com valores menores que o anterior.
   
  arquivo = sd.open(nomeArquivo, FILE_WRITE);
  arquivo.print(horario);
  arquivo.print(",");
  arquivo.print(tempo);
  arquivo.print(",");
  arquivo.println(PesoInst*(1000/Densidade));
  arquivo.close();

  coletamomento = PesoInst;

  if (tempo > 300) {
    tone(pino_buzzer,Freq_Som,10000);  
    delay(2000);
    tone(pino_buzzer,Freq_Som,10000);  
    delay(2000); 
    tone(pino_buzzer,Freq_Som, 10000);  
      
    sleep.pwrDownMode(); // Ativa modo repouso.
    sleep.sleepDelay(sleepTime); // Repouso por 7 dias.
  }  
  
  digitalWrite(Pino_LED, LOW); // Desliga o LED.

  tempo = tempo + 0.1;

  delay(Delay);
}

void calibracao() { // Rotina de calibração da célula de carga.
  
  som_500(); 
  delay(2500); 
  tone(pino_buzzer,Freq_Som, 3000);  
  delay(13000); 


// Tara sem peso:


  for (int ii=0;ii<int(NumeroAfericoes);ii++) 
  {  
    delay(10);
    RawZero+=hx711.read();
  }
  
  RawZero/=long(NumeroAfericoes);

  tone(pino_buzzer,Freq_Som,3000);  
  delay(13000); 

// Tara com frasco

  for (int ii=0;ii<int(NumeroAfericoes);ii++) 
  {  
    delay(10);
    RawTara+=hx711.read();
  }

  RawTara/=long(NumeroAfericoes); // Realiza a média das aferições pré-determinadas em ciclos de 10 milissegudos.
  
  tone(pino_buzzer,Freq_Som,3000);  
  delay(33000); 

// Peso de calibração:

  int ii = 1;

  while(true)
  {
    if (hx711.read()<RawTara+10000)
    {  
    } 
    else 
    {
      ii++;
      delay(2000);
      for (int jj=0;jj<int(NumeroAfericoes);jj++)
      {
        RawCalibracao+=hx711.read();
      }
      RawCalibracao/=long(NumeroAfericoes);
      break;
     }
  }

  sd.remove("/OPENFLOW/config.ini");
  arquivo.close();
  arquivo = sd.open("/OPENFLOW/config.ini", FILE_WRITE);
  arquivo.println("[CONFIGURACAO]");
  arquivo.println("Zero = " + String(RawZero));
  arquivo.println("Tara = " + String(RawTara));
  arquivo.println("Valor = " + String(RawCalibracao));
  arquivo.close();

  som_500();
  while(1);
}

void criarNomeArquivoDiretorio() { // Rotina para criar nome da pasta e do arquivo com base na data e hora atuais.  
  
  DateTime now = rtc.now();

  if (now.day() >=0 && now.day() < 10) { zerodia = "0"; } else {zerodia = "";};
  if (now.month() >=0 && now.month() <10) { zeromes = "0"; } else {zeromes = "";};
  if (now.hour() >=0 && now.hour() < 10) { zerohora = "0"; } else {zerohora = "";};
  if (now.minute() >=0 && now.minute() < 10) { zeromin = "0"; } else {zeromin = "";};
  if (now.second() >=0 && now.second() < 10) { zeroseg = "0"; } else {zeroseg = "";};

  String nomeDiretorio;
  nomeDiretorio = "/OPENFLOW/DADOS/" + zerodia + String(now.day()) + '-' + zeromes + String(now.month()) + '-' + String(now.year() - 2000);
  nomeArquivo = nomeDiretorio + "/" + zerohora + String(now.hour()) + '-' + zeromin + String(now.minute()) + '-' + zeroseg + String(now.second()) + '.' + 'C' + 'S' + 'V';
  const char diretorio[27];
  nomeDiretorio.toCharArray(diretorio, sizeof(diretorio));
  sd.mkdir(diretorio); 
}

void criarDiretorioSistema() { //Cria nome da pasta caso não exista. 

  if (!sd.exists("/OPENFLOW")) {
    sd.mkdir("/OPENFLOW");
    sd.mkdir("OPENFLOW/DADOS");
  } else { }
}
  
void som_200() {

  tone(pino_buzzer,Freq_Som,200);  
  delay(400);     
}

void som_500() {
  for (int i=0; i<5; i++) {
    tone(pino_buzzer,Freq_Som,500);  
    delay(1000);
  } 
}

void led_pisca() {
  digitalWrite(Pino_LED, HIGH); // Liga LED;
  delay(100);
  digitalWrite(Pino_LED, LOW); // Desliga LED;
  delay(100);
}
