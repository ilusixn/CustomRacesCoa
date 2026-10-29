# Scripts Windows

Ce sont les scripts qui ont servi à compiler et installer le module sur le poste de développement. Ils contiennent des chemins propres à ce poste (`M:\conquest\...`, `C:\coa-build\vcpkg`) : **adaptez-les** avant de les lancer, ils ne sont qu'un exemple.

- `BUILD_BARDE.bat` : compile `worldserver` avec le module (Visual Studio, CMake, Ninja, vcpkg).
- `INSTALL_BARDE.bat` + `install_barde.py` : installent le serveur compilé, la configuration, les DBC et le patch client, la base de données et l'addon dans une copie de serveur CoA.
- `COPIER_SERVEUR.bat` : fait une copie du serveur d'origine pour y installer le module.
- `mysql-version.patch` : contournement propre à un serveur précis (version de libmysql).

Le manuel général est dans [INSTALL.md](../../INSTALL.md) (anglais) et [INSTALL.fr.md](../../INSTALL.fr.md) (français).

These are the scripts used to build and install the module on the development machine. They contain paths specific to that machine: **adapt them** before running; they are only an example.
