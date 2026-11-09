# TegraScript / LockSmith-RCM

Documentation for the scripting engine based on the **TegraExplorer V4** engine, including the functions and extensions available in LockSmith-RCM.

---

## Variables

### Assignment

TegraScript variables do not require an explicit type declaration.

```text
variable = function(arg1, arg2)
```

Variables are global to the script: a variable created in a user-provided function can be used elsewhere in the script.

```text
value = 10
print(value)
```

Variables can be used in place of literal values:

```text
three = 3
print(1 + 2 + three)
```

### User-provided functions

User-provided functions do not directly take arguments:

```text
helloWorld = {
	print("hello world!")
}

helloWorld()
```

When a user-provided function is called, expressions placed between `()` are evaluated before the function is executed and their result is discarded. This is a historical TegraScript v2 behavior that is kept for backwards compatibility:

```text
a = {
	b = b + 1
}

a(b = 5)
```

Normal assignment works as well:

```text
a = {
	b = b + 1
}

b = 5
a()
```

### Variable members

Most variable types have function members.

```text
three = 3
three.print()
```

---

## Arrays

TegraScript has four array types:

- `StringArray`
- `IntegerArray`
- `ByteArray`
- `EmptyArray`

Examples:

```text
strings = ["a", "b", "c"]
numbers = [1, 2, 3, 4, 5]
bytes = ["BYTE[]", 1, 2, 3, 4, 5]
empty = []
```

An `EmptyArray` is converted into a typed array when a compatible integer or string is added.

Integers added to a `ByteArray` are converted to bytes.

### Array access

Array elements can be accessed using an index:

```text
numbers = [1, 2, 3, 4, 5]
print(numbers[1])
```

This prints `2`.

The `array[index]` syntax is internally converted to `array.get(index)`, so both forms are valid:

```text
print(numbers[1])
print(numbers.get(1))
```

### Modifying arrays

An array element can be modified with:

```text
a = [1, 2, 3].copy()
a[0] = 69
```

Static arrays such as `[1, 2, 3]` are read-only. They must be copied with `.copy()` before being modified.

Arrays containing expressions or variables, such as `[a, b, c]`, are evaluated when execution reaches the expression and do not require `.copy()` solely because of being non-static.

### ArrayReference

Functions such as `.slice()` can return an `ArrayReference`. An array reference is a view of the original array rather than an independent copy.

An `ArrayReference` can be projected using `.project()`. This is not memory-safe after the underlying array has been changed; it is best to project the reference again before each use if the array may have changed.

---

## Logical integer inversion

The `!` operator logically inverts an integer:

```text
!1
```

returns `0`, while:

```text
!0
```

returns `1`.

The equivalent member function is:

```text
1.not()
```

---

## Operator precedence

TegraScript has **no operator precedence**. Operators are evaluated from left to right.

Therefore:

```text
println(2 + 2 * 2)
```

evaluates as:

```text
(2 + 2) * 2
```

and produces `8`.

Parentheses can be used to explicitly control evaluation order:

```text
println(2 + (2 * 2))
```

produces `6`.

---

# Flow control

TegraScript provides the following flow-control functions.

### `if(Integer)`

Executes the following user-provided function when the value is non-zero.

```text
if (1) {
	print("Hello world!")
}
```

`if()` returns an object that can be used to add an `else` block:

```text
if (0) {
	print("yes")
}.else() {
	print("no")
}
```

### `while(Integer)`

Continues executing the associated user-provided function while the condition evaluates to a non-zero integer.

```text
i = 0

while (i < 10) {
	println(i)
	i = i + 1
}
```

### `break()`

Breaks out of the current `while` or `foreach` loop.

```text
while (1) {
	break()
}

println("The loop broke")
```

### `exit()`

Exits the script immediately.

```text
exit()
```

Unlike a runtime error, `exit()` is considered a normal script exit.

---

# Comments and requirements

Comments start with `#` and continue until the end of the line.

```text
# This is a comment
print("Hello")
```

Some `#REQUIRE` lines are interpreted by the engine as script requirements.

## `#REQUIRE VER`

Specifies the minimum engine version required to run the script.

```text
#REQUIRE VER 4.0.0
```

The version is specified as `major.minor.patch`.

## `#REQUIRE MINERVA`

Requires the extended memory provided by Minerva.

This should be used when working with large files, saves, or generally large scripts.

```text
#REQUIRE MINERVA
```

## `#REQUIRE KEYS`

Requires the necessary keys to be available.

```text
#REQUIRE KEYS
```

## `#REQUIRE SD`

Requires the SD card to be available and mounted.

```text
#REQUIRE SD
```

---

# Classes

## Integer

TegraScript integers are stored using 8 bytes.

### Definition

```text
10
0x10
```

`10` is `10`, while `0x10` is `16`.

### Operators

| Operator | RVal | Result | Description |
|---|---|---|---|
| `+` | Integer | Integer | Addition |
| `-` | Integer | Integer | Subtraction |
| `*` | Integer | Integer | Multiplication |
| `/` | Integer | Integer | Division |
| `%` | Integer | Integer | Modulo |
| `<` | Integer | Integer | Less than |
| `>` | Integer | Integer | Greater than |
| `<=` | Integer | Integer | Less than or equal to |
| `>=` | Integer | Integer | Greater than or equal to |
| `==` | Integer | Integer | Equality |
| `!=` | Integer | Integer | Inequality |
| `&&` | Integer | Integer | Logical AND |
| `\|\|` | Integer | Integer | Logical OR |
| `&` | Integer | Integer | Bitwise AND |
| `\|` | Integer | Integer | Bitwise OR |
| `<<` | Integer | Integer | Left bitshift |
| `>>` | Integer | Integer | Right bitshift |

### Members

| Member | Arguments | Result | Description |
|---|---|---|---|
| `print()` | — | None | Prints the integer |
| `not()` | — | Integer | Logically inverts the integer |
| `str()` | — | String | Converts the integer to a string |

---

## String

TegraScript strings are immutable.

### Definition

```text
"Hello"
"\r\n"
```

Escape sequences such as `\n` and `\r` can be used in strings.

### Operators

| Operator | RVal | Result | Description |
|---|---|---|---|
| `+` | String | String | Concatenates two strings |
| `-` | Integer | String | Reduces the string size by the specified integer amount |
| `==` | String | Integer | Checks string equality |
| `!=` | String | Integer | Checks string inequality |
| `/` | String | String Array | Splits the left string using the right string as separator |

### Members

| Member | Arguments | Result | Description |
|---|---|---|---|
| `print()` | — | None | Prints the string |
| `len()` | — | Integer | Gets the length of the string |
| `bytes()` | — | ByteArray | Returns the string as a byte array |
| `get()` | Integer | String | Returns the character at the given index |
| `split()` | String | StringArray | Splits the string using the specified separator |

Example:

```text
path = "sd:/folder/file.bin"
parts = path.split("/")
```

---

## Array

Arrays can be `IntegerArray`, `StringArray`, `ByteArray`, or `EmptyArray`.

### Members

| Member | Arguments | Result | Description |
|---|---|---|---|
| `get()` | Integer | String or Integer | Gets the element at the specified index |
| `len()` | — | Integer | Gets the length of the array |
| `slice()` | Integer, Integer | ArrayReference | Returns a read-only array slice |
| `foreach()` | String, Function | None | Iterates through the array and stores each element in the specified variable |
| `copy()` | — | Array | Creates a copy of the array |
| `set()` | Integer, compatible value | None | Sets the array element at the specified index |
| `add()` | Compatible value | None | Adds a value to the array |
| `+` | Compatible value | None | Adds a value to the array |
| `-` | Integer | None | Removes the specified number of elements from the end of the array |
| `contains()` | Compatible value | Integer | Returns the index + 1 of the matching element, or `0` if not found |
| `bytestostr()` | — | String | Converts a `ByteArray` to a string |
| `==` | Compatible array | Integer | Compares two arrays |
| `find()` | Compatible array | Integer | Searches for a value/sub-array and returns its offset, or `-1` if not found |

### `foreach()`

The first argument is the name of the variable that receives each element.

```text
numbers = [1, 2, 3]

numbers.foreach("value") {
	println(value)
}
```

### `contains()`

`contains()` returns `0` when the value is not found. When the value is found, it returns its index + 1.

```text
numbers = [10, 20, 30]

print(numbers.contains(20))
```

prints `2`.

This convention allows index `0` to be distinguished from a failed search.

---

## Dictionary

An empty dictionary can be created with `dict()`:

```text
a = dict()
a.b = 1

print(a.b)
```

Dictionaries are also used as return values by several standard-library functions.

---

## Save

A save can be opened with:

```text
save = readsave("path/to/save")
```

Opening and processing a save can consume a significant amount of memory.

### Members

| Member | Arguments | Result | Description |
|---|---|---|---|
| `read()` | String | ByteArray | Reads a file from the save |
| `write()` | String, ByteArray | Integer | Writes a file to the save. `0` indicates success |
| `commit()` | — | None | Writes the changes and signs the save |

Example:

```text
save = readsave("bis:/save/8000000000000120")
data = save.read("path/to/file")
save.write("path/to/file", data)
save.commit()
```

---

# Standard library

## Flow control

| Function | Arguments | Result | Description | Example |
|---|---|---|---|---|
| `if` | Integer, Function | Elseable | Executes the following function if the integer is non-zero | `if (1) { print("Hello") }` |
| `while` | Integer, Function | None | Executes the function while the condition is non-zero | `i = 0 while (i < 10) { println(i) i = i + 1 }` |
| `exit` | — | None | Exits the script normally | `exit()` |
| `break` | — | None | Breaks the current loop | `while (1) { break() }` |

---

## Object creation

### `readsave`

```text
readsave(String path)
```

Opens a save and returns a `Save` object.

```text
save = readsave("bis:/save/8000000000000120")
```

### `dict`

```text
dict()
```

Creates an empty dictionary.

```text
a = dict()
a.value = 123
```

---

## Display and utility functions

### `print`

```text
print(...)
```

Accepts any number of `Integer` and `String` arguments.

Arguments are printed with spaces between them.

```text
print("Value:", 123)
```

### `println`

Same as `print()`, then prints a newline.

```text
println("Hello", 123)
```

### `orientation`

```text
orientation()
orientation(mode)
```

Gets or changes the payload display orientation.

- `0`: landscape
- `1`: portrait
- `orientation()` returns the current mode.
- `orientation(mode)` returns `0` on success and `-1` for an invalid mode.

Every script starts in landscape mode for compatibility with TegraExplorer V4 scripts. A script can change the orientation at any time. Changing orientation clears the display and resets the text cursor to the top-left corner.

Example:

```text
orientation(1)
clear()
println("Portrait mode")
orientation(0)
```

### `printpos`

```text
printpos(x, y)
```

Sets the text cursor position using the current logical display orientation.

- In landscape mode the logical display is `1280 x 720`, giving 80 columns and 45 rows at 16-pixel font size.
- In portrait mode the logical display is `720 x 1280`, giving 45 columns and 80 rows at 16-pixel font size.

```text
printpos(20, 20)
```

### `setpixel`

```text
setpixel(x, y, color)
```

Draws a pixel.

- `x` and `y` use the current logical display orientation.
- Landscape: `x` `0..1279`, `y` `0..719`.
- Portrait: `x` `0..719`, `y` `0..1279`.
- `color`: RGB color in `0xRRGGBB` format

```text
setpixel(0, 0, 0xFFFFFF)
```

### `setpixels`

```text
setpixels(x0, y0, x1, y1, color)
```

Draws/fills the rectangle defined by the two points.

```text
setpixels(0, 0, 1279, 719, 0xFF0000)
```

The coordinates use the same logical dimensions as `setpixel()`.

### `clear`

Clears the screen and resets the cursor to the top-left corner.

```text
clear()
```

### `color`

```text
color(0xRRGGBB)
```

Sets the text color.

```text
color(0x00FF00)
print("Green text!")
```

### `timer`

Returns the current system time in milliseconds.

```text
start = timer()
# operation to measure
print("Time taken:", timer() - start)
```

### `cwd`

Returns the current working directory of the script.

For a built-in script, the current working directory is normally:

```text
sd:/
```

For a script loaded from a file, the current working directory corresponds to the directory containing the script.

```text
print(cwd())
```

### `combinepath`

```text
combinepath(String, String, ...)
```

Combines multiple path components.

```text
path = combinepath("sd:/", "tegraexplorer", "firmware")
```

Result:

```text
sd:/tegraexplorer/firmware
```

### `escapepath`

```text
escapepath(String)
```

Returns the parent directory of the specified path.

```text
escapepath("sd:/tegraexplorer/firmware")
```

returns:

```text
sd:/tegraexplorer
```

---

## Input

### `pause`

```text
pause()
```

Waits for user input and returns a `Dictionary` containing:

- `a`
- `b`
- `x`
- `y`
- `down`
- `up`
- `right`
- `left`
- `power`
- `volplus`
- `volminus`
- `raw`

Example:

```text
p = pause()

if (p.a) {
	println("A was pressed!")
}
```

### `pause(Integer)`

Allows a custom input bitmask to be supplied.

```text
pause(0xC)
```

The script waits for input matching the supplied mask.

The available mask values are defined by the HID mask constants in the engine.

### `hidread`

```text
hidread()
```

Reads the current button state without waiting for a new input event.

The result is the same type of `Dictionary` returned by `pause()`.

---

## Menus

### `menu(StringArray, Integer)`

Creates a menu from a string array.

- argument `0`: menu options
- argument `1`: initial position

The function returns the index of the selected option when A or Power is pressed.

```text
idx = menu(["1", "2", "3"], 0)

if (idx >= 0) {
	println(idx, "was pressed!")
}
```

### `menu(StringArray, Integer, IntegerArray)`

Provides additional display options for each menu entry. This existing form remains fully supported; scripts that use it do not need to be changed.

- argument `0`: menu options
- argument `1`: initial position
- argument `2`: color/options array

Each element of the third argument corresponds to the menu entry at the same index.

The lower three bytes contain the RGB color `0xRRGGBB`.

The upper byte is an option bitfield:

| Bit | Value | Function |
|---|---:|---|
| `0x1` | 1 | Skip entry |
| `0x2` | 2 | Hide entry |
| `0x4` | 4 | File icon |
| `0x8` | 8 | Folder icon |

Example:

```text
menu(
	["Option 1", "Option 2"],
	0,
	[0xFFFF0000, 0xFF00FF00]
)
```

### Optional menu margins

Menus can optionally reserve vertical space above and below the entries. The margins are expressed in pixels and are applied only to that menu call. Existing calls using two or three arguments keep their previous behavior, with both margins set to `0`.

Use four arguments when no per-entry options array is needed:

```text
idx = menu(["Option 1", "Option 2"], 0, 24, 32)
```

- argument `2`: top margin, in pixels
- argument `3`: bottom margin, in pixels

Use five arguments to combine the margins with the existing per-entry options array:

```text
idx = menu(
	["Option 1", "Option 2"],
	0,
	[0xFFFF0000, 0xFF00FF00],
	24,
	32
)
```

- argument `3`: top margin, in pixels
- argument `4`: bottom margin, in pixels

Negative margins are treated as `0`; values larger than the portrait reference height are capped to that height. Margins are defined in portrait-height pixels. In landscape orientation, the menu scales them by the ratio between the current logical screen height and the portrait reference height, so the reserved area remains proportional.

### Optional fixed menu caption

A caption can be supplied as the final argument. It is shown at the top of the menu, followed by one blank line, and does not scroll with the selectable entries. Existing calls without a caption are unchanged. A null or empty caption is not displayed.

Caption without entry options or margins:

```text
idx = menu(["Option 1", "Option 2"], 0, "Choose an option")
```

Caption with entry options:

```text
idx = menu(["Option 1", "Option 2"], 0, [0xFFFFFFFF, 0xFF00FF00], "Choose an option")
```

Caption with margins:

```text
idx = menu(["Option 1", "Option 2"], 0, 24, 32, "Choose an option")
```

Caption with both entry options and margins:

```text
idx = menu(
	["Option 1", "Option 2"],
	0,
	[0xFFFFFFFF, 0xFF00FF00],
	24,
	32,
	"Choose an option"
)
```

When used with margins, the caption occupies part of the available vertical space; the top margin remains reserved above the caption and the bottom margin remains reserved below the entries. The menu dynamically accounts for the standard LockSmith-RCM title and footer in normal menus. TegraExplorer script menus do not draw those elements, so their full logical screen height is available apart from the supplied margins and the caption. The menu also shows `<` above the entries when more selectable entries are available above, and `>` below them when more are available below.

When a screenshot is taken from a menu, its filename base is derived from the caption. Characters unsafe for filenames are removed or converted to separators. If the caption is empty or absent, the base name is `unknown_menu` for a normal menu and `unknown_script_menu` for a TegraExplorer script menu. Existing screenshot collision handling adds a numeric suffix if the base filename is already in use.

---

# NAND / eMMC

The following functions can operate on the system NAND and emuNAND.

The script engine preserves the NAND context around script operations. `mountsys()` and `mountemu()` select the NAND context used by subsequent script NAND/file operations.

## `emu`

```text
emu()
```

Returns:

- `1` if an emuNAND is currently enabled
- `0` otherwise

```text
if (emu()) {
	println("emuNAND enabled")
}
```

## `mountsys`

### Standard form

```text
mountsys(String partition)
```

Mounts a partition from the system NAND.

Returns:

- `0`: success
- `1`: failure

Example:

```text
result = mountsys("SYSTEM")
```

### Optional parameter form

```text
mountsys(String partition, Integer bisCacheEnable)
```

The third argument controls the `bis_cache_enable` parameter passed to the mount operation.

- `0`: disable the BIS cache processing controlled by this parameter
- `1`: enable it

The one-argument form keeps the default behavior with the parameter enabled.

```text
result = mountsys("USER", 0)
```

---

## `mountemu`

### Standard form

```text
mountemu(String partition)
```

Same as `mountsys()`, but targets the emuNAND.

```text
result = mountemu("SYSTEM")
```

### Optional parameter form

```text
mountemu(String partition, Integer bisCacheEnable)
```

The third argument controls the `bis_cache_enable` parameter passed to the mount operation.

- `0`: disable the BIS cache processing controlled by this parameter
- `1`: enable it

The one-argument form keeps the default behavior with the parameter enabled.

```text
result = mountemu("USER", 0)
```

---

---

## `emmcread`

### Standard form

```text
emmcread(String destination, String partition)
```

Dumps a partition from the system NAND to a file.

- argument `0`: destination file
- argument `1`: partition to dump

Returns:

- `0`: success
- non-zero: failure

```text
emmcread("sd:/PRODINFO", "PRODINFO")
```

### Optional parameter form

```text
emmcread(String destination, String partition, Integer bisReadOrWriteEnable)
```

The third argument controls the `bis_read_or_write_enable` parameter passed to the dump operation.

- `0`: disable the BIS processing controlled by this parameter
- `1`: enable it

The two-argument form keeps the default behavior with the parameter enabled.

```text
emmcread("sd:/PRODINFO", "PRODINFO", 0)
```

Note: If a partition is more than 4 GB and dumped to a SD in FAT32 it will be dumped with a split of files of 2 GO and sufix ".00", ".01", ".02" will be added to the filename passed. If an other splited dump of this form exists and if the actual dump is smaller than the dump present the pieces of the holder dump will be removed.

---

## `emmcwrite`

### Standard form

```text
emmcwrite(String source, String partition)
```

Restores a file to a partition on the system NAND.

- argument `0`: source file
- argument `1`: target partition

```text
emmcwrite("sd:/PRODINFO", "PRODINFO")
```

### Optional parameter form

```text
emmcwrite(String source, String partition, Integer bisReadOrWriteEnable)
```

The third argument controls `bis_read_or_write_enable`.

- `0`: disabled
- `1`: enabled

The two-argument form keeps the default behavior with the parameter enabled.

```text
emmcwrite("sd:/PRODINFO", "PRODINFO", 0)
```

Note: If a splited dump is detected it will have the priority over a single file partition, for example if you pass the param "sd:/USER.bin" for the path of the dump used to restore and if a splited dump exists it will be detected ans will be used. A splited dump can be named, for USER partition for example, "USER.bin.00", "USER00", "USER0", "USER.00" and "USER.00.bin".

Note 2: The restoration can work with a smaller file than the partition but not with a bigger, even with a splited dump.

---

## `emummcread`

### Standard form

```text
emummcread(String destination, String partition)
```

Dumps a partition from the emuNAND.

```text
emummcread("sd:/PRODINFO", "PRODINFO")
```

### Optional parameter form

```text
emummcread(String destination, String partition, Integer bisReadOrWriteEnable)
```

The third argument controls `bis_read_or_write_enable`.

- `0`: disabled
- `1`: enabled

```text
emummcread("sd:/PRODINFO", "PRODINFO", 0)
```

Note: If a partition is more than 4 GB and dumped to a SD in FAT32 it will be dumped with a split of files of 2 GO and sufix ".00", ".01", ".02" will be added to the filename passed. If an other splited dump of this form exists and if the actual dump is smaller than the dump present the pieces of the holder dump will be removed.

---

## `emummcwrite`

### Standard form

```text
emummcwrite(String source, String partition)
```

Restores a file to a partition on the emuNAND.

```text
emummcwrite("sd:/PRODINFO", "PRODINFO")
```

### Optional parameter form

```text
emummcwrite(String source, String partition, Integer bisReadOrWriteEnable)
```

The third argument controls `bis_read_or_write_enable`.

- `0`: disabled
- `1`: enabled

```text
emummcwrite("sd:/PRODINFO", "PRODINFO", 0)
```

Note: If a splited dump is detected it will have the priority over a single file partition, for example if you pass the param "sd:/USER.bin" for the path of the dump used to restore and if a splited dump exists it will be detected ans will be used. A splited dump can be named, for USER partition for example, "USER.bin.00", "USER00", "USER0", "USER.00" and "USER.00.bin".

Note 2: The restoration can work with a smaller file than the partition but not with a bigger, even with a splited dump.

---

## `ncatype`

```text
ncatype(String path)
```

Returns the NCA type of the specified file.

Returns `-1` on error.

```text
type = ncatype("sd:/title.nca")
```

---

## `fuse_patched`

```text
fuse_patched()
```

Returns the detected RCM fuse-patch state.

```text
if (fuse_patched()) {
	println("RCM is patched")
}
```

## `fuse_hwtype`

```text
fuse_hwtype()
```

Returns the hardware type detected from the fuses.

```text
hw = fuse_hwtype()
```

---

# File system

## `readdir`

```text
readdir(String path)
```

Reads a directory and returns a `Dictionary` containing:

- `result`: result code; `0` indicates success
- `files`: `StringArray` containing files
- `folders`: `StringArray` containing folders
- `fileSizes`: `IntegerArray` containing file sizes

Entries in `fileSizes` correspond to entries in `files` by index.

Example:

```text
dir = readdir("sd:/")

if (dir.result) {
	exit()
}

dir.files.foreach("file") {
	println(file)
}
```

## `deldir`

```text
deldir(String path)
```

Deletes a directory.

The function prints the entries being deleted during the operation.

Returns `0` on success.

```text
deldir("sd:/sxos")
```

## `mkdir`

```text
mkdir(String path)
```

Creates a directory.

Returns `0` on success.

```text
mkdir("sd:/tegraexplorer")
```

## `copydir`

```text
copydir(String source, String destination)
```

Copies a directory into another directory.

```text
copydir("sd:/tegraexplorer", "sd:/switch")
```

This copies `sd:/tegraexplorer` into `sd:/switch`.

## `copyfile`

```text
copyfile(String source, String destination)
```

Copies a file.

```text
copyfile("sd:/hbmenu.nro", "sd:/hbmenu_2.nro")
```

## `movefile`

```text
movefile(String source, String destination)
```

Moves or renames a file or directory.

```text
movefile("sd:/hbmenu.nro", "sd:/lol.nro")
```

## `delfile`

```text
delfile(String path)
```

Deletes a file.

Returns `0` on success.

```text
delfile("sd:/boot.dat")
```

## `readfile`

```text
readfile(String path)
```

Reads an entire file and returns its contents as a `ByteArray`.

This can be a memory-intensive operation.

A read error raises a fatal script error.

```text
data = readfile("sd:/hbmenu.nro")
```

## `writefile`

```text
writefile(String path, ByteArray data)
```

Writes a `ByteArray` to a file.

Returns `0` on success.

```text
writefile("sd:/file.bin", ["BYTE[]", 1, 2, 3])
```

## `fsexists`

```text
fsexists(String path)
```

Checks whether a file or directory exists.

Returns:

- `1`: exists
- `0`: does not exist

```text
if (fsexists("sd:/boot.dat")) {
	println("File exists")
}
```

---

# Payload

## `payload`

```text
payload(String path)
```

Boots the specified payload.

```text
payload("sd:/atmosphere/reboot_payload.bin")
```

Returns an integer on failure.

---

# Complete examples

## List files in a directory

```text
#REQUIRE SD

files = readdir("sd:/")

if (files.result) {
	println("Unable to read directory")
	exit()
}

files.files.foreach("file") {
	println(file)
}
```

## Simple menu

```text
clear()

choices = [
	"Dump PRODINFO",
	"Quit"
]

choice = menu(choices, 0)

if (choice == 0) {
	println("Dumping PRODINFO...")
	result = emmcread("sd:/PRODINFO", "PRODINFO")

	if (result) {
		println("Dump failed")
	}
}

if (choice == -1) {
	println("Cancelled")
}
```

## Dump without the BIS processing controlled by `bis_read_or_write_enable`

```text
result = emmcread(
	"sd:/PRODINFO.bin",
	"PRODINFO",
	0
)

if (result) {
	println("Dump failed")
}
```

## Select SYSNAND / EMUNAND

```text
choice = menu(["SYSNAND", "EMUNAND"], 0)

if (choice == 0) {
	mountsys("SYSTEM")
}
else {
	if (choice == 1) {
		mountemu("SYSTEM")
	}
}
```

---

# Important notes

- Array indices start at `0`.
- Variables are global to the script.
- Expressions are evaluated from left to right; there is no implicit mathematical operator precedence.
- Parentheses should be used whenever a specific evaluation order is required.
- Static arrays are read-only and must be copied with `.copy()` before modification.
- `readfile()` and `readsave()` can consume a significant amount of memory.
- `exit()` terminates the script normally; `break()` is intended for `while` and `foreach` loops.
- Functions with multiple signatures are selected according to the number and types of their arguments.
- The three-argument forms of `emmcread()`, `emmcwrite()`, `emummcread()` and `emummcwrite()` use the third argument to control `bis_read_or_write_enable`. Omitting the argument keeps the default behavior.
