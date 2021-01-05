# Return Codes

| Value | Description
| -1    | An error occured. Check the actual result for more informations. |
| 0     | Method was executed fine. |
| 1     | The method could not fill the entire buffer to the end. Use the returned value with method `?` until a different return code got returned to get the full result. |

# Methods

*Note: in the following table, required parameters are marked in **bold***

| Name  | Returns                          |          arg0          |          arg1           |                  arg2                |
|-------|----------------------------------|------------------------|-------------------------|--------------------------------------|
| ?     | Partial result of the prior call | **long-result** id     |                         |                                      |
| read  | *ARRAY* of *SCALAR*s or *STRING* | **file-path** *STRING* |    read-mode *STRING*   |                                      |
| write |             *NOTHING*            | **file-path** *STRING* | **write-mode** *STRING* | **contents** **STRING** or **ARRAY** |
| list  |   *ARRAY* of *file-descriptor*   | **dir-path** *STRING*  |      filter *STRING*    |                                      |


*Note: following table describes the different "types" used above. It is assumed that one knows the basic arma types, written in UPPERCASE*

| Type            | Description                                     |
|-----------------|-------------------------------------------------|
| long-result     | A long-result is returned, whenever the result is too large to fit in a single `callExtension` call. Use it with the Method `?` to receive the full contents. |
| file-path       | A path to a valid, existing file.               |
| dir-path        | A path to a valid, existing directory (folder). |
| file-descriptor | An array, consisting of two strings. First string always will be either `d` (Directory) or `f` (File). Second string is the path. Example: `["f", "C:/path/to/file.sqf"]` |
| read-mode       | A combination of the characters <ul><li>`b` (binary - reads the file in, in binary mode, returning an *ARRAY* of *SCALAR*s)</li><li>`i` (input - automatically set; Tells that you want to read a file)</li></ul>|
| write-mode      | A combination of the characters <ul><li>`t` (truncate - clear/create file and write)</li><li>`a` (truncate - clear/create file and write)</li><li>`o` (input - automatically set; Tells that you want to read a file)</li><li>`b` (binary - writes the file out, in binary mode, expecting an *ARRAY* of *SCALAR*s as input)</li></ul>|
