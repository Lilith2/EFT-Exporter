# EFT-Exporter
 Notepad++ plugin to format txt into SDK.cs lone format

 #### Known Bug
 Export Selection as individual file will accept infinite classes and put them into file which names the file every single class name at once seperate by underscores. Not a big deal for small selections but will screw you with big ones.

# Install Steps
- Find your Notepad++\plugins folder
- Create folder with same name as DLL
- put DLL into folder
- launch notepad++, goto plugins menu, should see your plugin in the list

# How to use?
- Make sure you have a valid Uninspect dump.txt file load into notepad++
- If you highlight / select a class from the file, you can use the "Export Selection to Master SDK" feature. It will be auto-formatted to C# structures that match Lone's format into a file that will constantly append new selections. Then you can copy paste it into SDK.cs without errors.
- "Export Selection as individual file" does exactly what Export Selection to Master SDK does except it names the file to the class you've selected and should only contain that exact class. This feature is intended for organized sharing of information. Not very useful otherwise.
- "Export entire file to SDK" will convert the entire dump into lone friendly C# format. You should be able to then replace your sdk.cs file with this one.
