ABANDONED (but will stay for messy posperity :)

quilte
------
![Screenshot](https://raw.github.com/ohwgiles/quilte/master/screen.png)

- Qt terminal emulator
- no dependencies on Glob or Krud
- search buffer
- unicode
- save buffer
- tabs
- configurable

qvtermwidget
------------
- standalone terminal emulator widget
- based on LeoNerd's vterm project https://launchpad.net/libvterm/

compiling
---------

### Build libvterm

```
git clone https://github.com/neovim/libvterm.git
cd libvterm
git fetch
git checkout 212859a30de7506c5e56bdc39af6141ddacd68f7
make
make install PREFIX=/home/user/libroot
```

### Build quilte

Open quilte.pro in Qt Creator.

Select "Projects -> Build". Add to environment variables:
```
LIBVTERM_DIR=/home/user/libroot
```

current state
-------------
- functional and useful
- qvtermwidget code is a mess though
- supports (very basic) embedded Hebrew RTL
- supports input method entry

planned improvements
--------------------
- hide/show toolbars
- right-click displays menu bar
- enable hiding of menu bar and tab titles
- why doesn't vim change the title? it does in pangoterm
- why doesn't ctrl-L clear the terminal
- better RTL support
- background transparency in composited environments
- profiles

license
-------
MIT
