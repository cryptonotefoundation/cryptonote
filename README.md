Este es el código de referencia para el protocolo de criptomoneda CryptoNote .

Inicie su propia moneda de CryptoNote : CryptoNote Starter
Implementación de referencia CryptoNote : CryptoNoteCoin
Panel de discusión y soporte: Foro CryptoNote
CryptoNote forking cómo hacerlo
Preparación
Crear una cuenta en GitHub.com
Repositorio de CryptoNote de horquilla
Compre uno o dos servidores dedicados basados ​​en Ubuntu (al menos 2 Gb de RAM) para nodos semilla.
Primer paso. Dale un nombre a tu moneda
El buen nombre debe ser único. Verifique la singularidad con google y Map of Coins o cualquier otro servicio similar.

El nombre debe especificarse dos veces:

1. en el archivo src / CryptoNoteConfig.h - CRYPTONOTE_NAMEconstante

Ejemplo:

const char CRYPTONOTE_NAME[] = "furiouscoin";
2. en el archivo src / CMakeList.txt - set_property (daemon TARGET PROPERTY OUTPUT_NAME "YOURCOINNAME d ")

Ejemplo:

set_property(TARGET daemon PROPERTY OUTPUT_NAME "furiouscoind")
Nota: También debe cambiar el nombre de un repositorio.

Segundo paso. Lógica de emisión
1. Suministro total de dinero (src / CryptoNoteConfig.h)

Cantidad total de monedas a emitir. La mayoría de las monedas basadas en CryptoNote utilizan (uint64_t)(-1)(es igual a 18446744073709551616). Puede definir el número explícitamente (por ejemplo UINT64_C(858986905600000000)).

Ejemplo:

const uint64_t MONEY_SUPPLY = (uint64_t)(-1);
2. Curva de emisión (src / CryptoNoteConfig.h)

Ser predeterminado CryptoNote proporciona una fórmula de emisión con una ligera disminución de la recompensa de bloque con cada bloque. Esto es diferente de Bitcoin, donde las recompensas de bloque se dividen cada cuatro años.

EMISSION_SPEED_FACTORconstante define la pendiente de la curva de emisión. Este parámetro es requerido para calcular la recompensa del bloque.

Ejemplo:

const unsigned EMISSION_SPEED_FACTOR = 18;
3. Objetivo de dificultad (src / CryptoNoteConfig.h)

El objetivo de dificultad es un período de tiempo ideal entre bloques. En caso de que el tiempo promedio entre bloques sea menor que el objetivo de dificultad, la dificultad aumenta. El objetivo de dificultad se mide en segundos.

El objetivo de dificultad influye directamente en varios aspectos del comportamiento de la moneda:

velocidad de confirmación de transacción: cuanto mayor es el tiempo entre los bloques, más lenta es la confirmación de transacción
Velocidad de emisión: cuanto más largo es el tiempo entre los bloques, más lento es el proceso de emisión.
tasa de huérfanos: las cadenas con bloques muy rápidos tienen una mayor tasa de huérfanos
Para la mayoría de las monedas, el objetivo es 60 o 120 segundos.

Ejemplo:

const uint64_t DIFFICULTY_TARGET = 120;
4. Fórmula de recompensa de bloque

En caso de que no esté satisfecho con la implementación predeterminada de la lógica de recompensa en bloque de CryptoNote, también puede cambiarla. La implementación está en src/CryptoNoteCore/Currency.cpp:

bool Currency::getBlockReward(size_t medianSize, size_t currentBlockSize, uint64_t alreadyGeneratedCoins, uint64_t fee, uint64_t& reward, int64_t& emissionChange) const
Esta función tiene dos partes:

cálculo de la recompensa del bloque básico: uint64_t baseReward = (m_moneySupply - alreadyGeneratedCoins) >> m_emissionSpeedFactor;
cálculo de penalización de bloque grande: esta es la manera en que CryptoNote protege la cadena de bloques de los ataques de inundación de transacciones y preserva las oportunidades para el crecimiento orgánico de la red al mismo tiempo.
Solo la primera parte de esta función está directamente relacionada con la lógica de emisión. Puedes cambiarlo como quieras. Ver MonetaVerde y DuckNote como los ejemplos donde se modifica esta función.

Tercer paso. Redes
1. Puertos predeterminados para redes P2P y RPC (src / CryptoNoteConfig.h)

El puerto P2P es utilizado por los demonios para comunicarse entre ellos a través del protocolo P2P. El monedero RPC usa el puerto RPC para hablar con el demonio.

Es mejor elegir puertos que no sean utilizados por otro software o monedas. Ver las listas de puertos TCP conocidos:

http://www.speedguide.net/ports.php
http://www.networksorcery.com/enp/protocol/ip/ports00000.htm
http://keir.net/portlist.html
Ejemplo:

const int P2P_DEFAULT_PORT = 17236;
const int RPC_DEFAULT_PORT = 18236;
2. Identificador de red (src / P2p / P2pNetworks.h)

Este identificador se utiliza en paquetes de red para no mezclar dos redes de criptomonedas diferentes. Cambie todos los bytes a valores aleatorios para su red:

const static boost::uuids::uuid CRYPTONOTE_NETWORK = { { 0xA1, 0x1A, 0xA1, 0x1A, 0xA1, 0x0A, 0xA1, 0x0A, 0xA0, 0x1A, 0xA0, 0x1A, 0xA0, 0x1A, 0xA1, 0x1A } };
3. Nodos de semillas (src / CryptoNoteConfig.h)

Agregue las direcciones IP de sus nodos semilla.

Ejemplo:

const std::initializer_list<const char*> SEED_NODES = {
  "111.11.11.11:17236",
  "222.22.22.22:17236",
};
Cuarto paso. Tarifa de transacción y parámetros relacionados
1. Tarifa de transacción mínima (src / CryptoNoteConfig.h)

La tarifa mínima cero puede llevar a la inundación de transacciones. Las transacciones más baratas que la tarifa de transacción mínima no serían aceptadas por demonios. Valor de 100000 MINIMUM_FEEes generalmente suficiente.

Ejemplo:

const uint64_t MINIMUM_FEE = 100000;
2. Tamaño de bloque libre de penalización (src / CryptoNoteConfig.h)

CryptoNote protege la cadena contra la inundación de tx reduciendo la recompensa de bloque para bloques más grandes que el tamaño de bloque mediano. Sin embargo, esta regla se aplica a los bloques más grandes que los CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONEbytes.

Ejemplo:

const size_t CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE = 20000;
Quinto paso Prefijo de dirección
Puede elegir una letra (en algunos casos, varias letras) con la que comenzarán todas las direcciones públicas de la moneda. Se define por la CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIXconstante. Dado que las reglas para los prefijos de direcciones no son triviales, puede utilizar la herramienta de generador de prefijos .

Ejemplo:

const uint64_t CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX = 0xe9; // addresses start with "f"
Sexto paso Bloque de génesis
1. Construye los binarios con genes en blanco tx hex (src / CryptoNoteConfig.h)

Debes dejar en const char GENESIS_COINBASE_TX_HEX[]blanco y compilar los binarios sin él.

Ejemplo:

const char GENESIS_COINBASE_TX_HEX[] = "";
2. Inicia el demonio para imprimir el bloque de génesis.

Ejecuta tu demonio con --print-genesis-txargumento. Se imprimirá el hash de transacción de la base de bloques de genesis.

Ejemplo:

furiouscoind --print-genesis-tx
3. Copie el hash de la transacción impresa (src / CryptoNoteConfig.h)

Copie el hash tx que ha sido impreso por el demonio GENESIS_COINBASE_TX_HEXensrc/CryptoNoteConfig.h

Ejemplo:

const char GENESIS_COINBASE_TX_HEX[] = "013c01ff0001ffff...785a33d9ebdba68b0";
4. Recompila los binarios.

Recompila todo de nuevo. Su código de moneda está listo ahora. ¡Haz un anuncio para los usuarios potenciales y disfruta!

Edificio CryptoNote
En * nix
Dependencias: GCC 4.7.3 o posterior, CMake 2.8.6 o posterior y Boost 1.55.

Puede descargarlos de:

http://gcc.gnu.org/
http://www.cmake.org/
http://www.boost.org/
Alternativamente, puede ser posible instalarlos usando un administrador de paquetes.
Para compilar, cambie a un directorio donde se encuentra este archivo y ejecútelo make. Los ejecutables resultantes se pueden encontrar en build/release/src.

Opciones avanzadas:

Construcción paralela: ejecutar en make -j<number of threads>lugar de make.
Construcción de depuración: ejecutar make build-debug.
Conjunto de pruebas: ejecutar make test-releasepara ejecutar pruebas además de la construcción. Ejecutando make test-debughará lo mismo con la versión de depuración.
Construyendo con Clang: puede ser posible usar Clang en lugar de GCC, pero esto puede no funcionar en todas partes. Para construir, ejecute export CC=clang CXX=clang++antes de ejecutar make.
En Windows
Dependencias: MSVC 2013 o posterior, CMake 2.8.6 o posterior y Boost 1.55. Puede descargarlos de:

http://www.microsoft.com/
http://www.cmake.org/
http://www.boost.org/
Para compilar, cambie a un directorio donde se encuentra este archivo y ejecute estos comandos:

mkdir build
cd build
cmake -G "Visual Studio 12 Win64" ..
Y luego construir. ¡Buena suerte!
