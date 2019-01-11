MXVCP, 13.Dezember 2018 by M.Heiß
----------------------------------

Dieses Projekt implementiert einen COM-Port Treiber für Windows 95 (Windows 98 sollte auch gehen).
Die COM-Ports sind reine Software. Jeweils zwei COM-Ports sind "virtuell" miteinander verbunden. Alles
was in den einen COM-Port geschrieben wird, kommt am anderen COM-Port raus - und umgekehrt.
Die Anzahl der COM-Ports ist statisch festgelegt (siehe #define NUMBER_OF_PORTS in driver.c).

Anwendungsbeispiele:
--------------------
- Das kann man nutzen, um z.B. den seriellen Output eines Programms umzuleiten und direkt auf dem selben
Computer zu verarbeiten, etc.


Projektstruktur:
----------------
- Ordner "ddk": Enthält das Windws 95 Driver Development Kit (DDK). Muss nicht installiert werden,
   da alles benötigte daraus entnommen wurde und diesem Projekt beiliegt.
   (Installation via setup.exe hat auch nicht funktioniert, da das Windows SDK benötigt wird aber
    das habe ich nicht. Macht nix, denn man kann - wenn benötigt - die Daten einfach auf Festplatte
    kopieren.)
- Ordner "doc": Enthält Informationen zum Projekt. Die *.doc Dateien sind dem DDK entnommen.
- Ordner "inc32": Inklude-Dateien, dem DDK entnommen.
- Ordner "result": Enthält den fertigen Treiber (vxd-Datei) sowie eine inf- und install-Datei zum
   installieren des Treibers.
- Ordner "src": Hier liegt der Quellcode des Treibers.
- Ordner "toolset": Assembler und Linker, dem DDK entnommen.
   (Als Compiler habe ich den Microsoft Visual Studio 6 Compiler verwendet. Liegt diesem Projekt
    aber nicht bei.)
- "root" Ordner: Batch-Dateien dienen als Hilfsmittel zum Bauen des Treibers".


Treiber Bauen:
--------------
Die Datei "start.bat" setzt ein paar benötigte Umgebungsvariablen zu Compiler, Assembler und Linker.
Diese Datei muss entsprechend der "Umgebung" angepasst werden.
Mit Datei "make.bat" wird der Treiber gebaut.


COM-Port Installation via *.inf-Datei:
--------------------------------------
Funktioniert nur bedingt:
   1. Hardware-Assistent aus der Systemssteuerung starten
   2. Automatische Hardware-Erkennung NEIN wählen
   3. Hardware-Type "Anschlüsse (COM und LPT Ports)" wählen
   4. Geräte-Auswahl via "Diskette" und die mxvcp.inf Datei wählen
   5. Den virtuellen COM-Port mit "Weiter" installieren
Diese Prozedur für jeden COM-Port wiederholen. Dadurch werden in der Registry entsprechende einträge generiert.
Pro Port gibt es zwei "Keys" in der Registry:
   - Hardware Key: "HKEY_LOCAL_MACHINE\Enum\Root\Ports\xxxx" mit xxxx=0000..9999
      - Der Schlüssel "Driver" verweißt dabei auf den jeweiligen Software Key.
   - Software Key: "HKEY_LOCAL_MACHINE\System\CurrentControlSet\Services\Class\Ports\yyyy" mit yyyy=0000..9999

Da wie oben erwähnt, die Installation der Ports via *.inf-Datei nicht vollständig funktioniert,
müssen im Hardware Key noch zusätzliche Schlüssel hinzugefügt bzw. modifiziert werden:
   - "PortName"="COMn", mit n=1..X (z.B. COM3)
   - "FriendlyName"="minlux Virtual COM-Port (COMn)", mit n=1..X (z.B. COM3)
   - "DeviceDesc"="minlux Virtual COM-Port (COMn)", mit n=1..X (z.B. COM3)
   - "PairPortName"="COMm", mit m=1..X (z.B. COM4


COM-Port Installation via install.bat:
--------------------------------------
Via install.bat können 4 COM-Ports (2 COM-Port-Paare), COM3<->COM4 und COM5<->COM6 installiert werden.
Dazu werden die oben beschriebenen Schlüssel in die Registry importiert (vgl. Datei com3456.reg) und die
Treiber-Datei mxvcp.vxd nach %windir%\system kopiert (z.B. C:\windows\system).

Anmerkung:
   Die VXD unterstützt aktuell bis zu 6 COM-Ports. Sollen alle 6 COM-Ports installiert werden, muss die
   Installation entsprechend angepasst werden.



Ähnliche Projekte:
------------------
Für neuere Windows Versionen ab XP sollte das Com2Com Projekt die gleiche Funktionalität bieten
(http://com0com.sourceforge.net/).


Weitere Quellen:
----------------
Buch: Writing Windows VXDs and Device Drivers, Karen Hazzah
   https://de.scribd.com/document/23270029/Writing-VXDS-for-Windows

University of Virginia Computer Science, x86 Assembly Guide
   http://www.cs.virginia.edu/~evans/cs216/guides/x86.html

Microsoft Assembler Reference
   https://docs.microsoft.com/de-de/cpp/assembler/inline/inline-assembler?view=vs-2017

Registry edit via Command Line
   https://www.computerhope.com/issues/ch000848.htm
   https://www.robvanderwoude.com/amb_removereg.php

VXD Tutorial
   http://win32assembly.programminghorizon.com/vxd-tut1.html

