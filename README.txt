Objetivo:
Crear un programa con interfaz gráfica desarrollada sobre Wxwidgets y enfocada al procesamiento de imágenes en
 formato pgm a través de los parches que sean delimitados dentro del a interfaz. 
 Como resultado de la selección de la ventana de pixeles que se operan, será posible observar múltiples modificaciones de la imágen 
 sobre porciones especificas de la misma. 
 
 La interfaz incluye las siguientes funciones y operaciones:
 1. Cargar y Guardar imágen en formato PGM.
 2. Filtrado mediante convolución y/o modificación directa de pixeles como:
    a) Suavizado.
    b) Detección de bordes.
    c) Aumento de contraste.
    d) Inversión de imagen (negativo de la imagen).
 3. Historial de operaciones y acciones ejecutadas (log entry).
 4. Opción para deshacer y rehacer cada uno de los cambios de acuerdo al historial de modificaciones realizadas. 
 
 Las imágenes de entrada serán en formato PGM (Portable Graymap format) de tal forma que todo su
 procesamiento se haga considerando un único canal de color y facilitando el procesamiento trabajando con un
 byte por pixel.

 
 Pre-requisitos:
 Comandos de Linux utilizados para la configuración de wxWidgets desde un
 sub-sistema en windows (WSL) en su distibución de Ubuntu:

 1. Instalación de compilador GCC:
    $sudo apt update
    $sudo apt install build-essential
 2. Instalación de librerias GTK para graficos de wxWidgets:
    $sudo apt install libgtk-3-dev
    $sudo apt install libgl1-mesa-dev
    $sudo apt install libglu1-mesa-dev
    $sudo apt install libwebkit2gtk-4.0-dev
3. Instalación de wxWidgets desde el repositorio basado en la documentación disponible en https://github.com/wxWidgets/wxWidgets/blob/master/docs/gtk/install.md
   Los comandos suponen una distribución basada en Debian tal como lo es Ubuntu:
    $git clone --recurse-submodules https://github.com/wxWidgets/wxWidgets.git
    $cd wxWidgets/
    $mkdir buildgtk
    $cd buildgtk/
    $../configure --with-gtk
    (el numero sobre el comando siguiente coincide con los nucleos en el procesador que ejecutará el programa,
    esto puede consultarse con el comando "$nproc")
    $make -j16 

    
    //Para verificar que el proceso de instalación ha sido correcto hasta este punto, se puede ejecutar alguno de los codigos de prueba incluidos 
    //en los archivos instalados de wxWidgets, para el ejemplo más sencillo esto es:
        $cd samples/minimal
        $make
        $./minimal


Ejecución del proyecto:
Con la carpeta del proyecto localizada en una ruta X del sistema, se puede ejecutar 
a partir del archivo "Makefile" como:
    $cd X/proyecto/build
    $make
    $./proyecto
    
//En caso de que se muestren advertencias estilo 
    Gdk-Message: Unable to load sb_v_double_arrow from the cursor theme
//se puede ejecutar el siguiente comando para obtener el estilo de graficos que solicita el programa
    $sudo apt-get -y install adwaita-icon-theme-full


