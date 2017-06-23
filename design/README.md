This design archive shows the workflow of how the logo came from its svg original into eagle.

1. First a complete redraw (including symmetry achieved by dependencies and measurings) was done in Inventor.
2. Then the single sketches were exported as DXF and further overworked using QCAD
3. Afterwards the awesome [import-dxf v1.6](https://github.com/erikwilson/import-dxf/) tool was used, to get the DXF files into eagle.
4. Sometimes these need to be slightly overworked. But more problematic is, that we mostly want to import the lines, curves and polylines as a polygon.
   So I decided to write a rather simple script ([polysignals.py](03_polysignals/polysignals.py)) which can convert closed eagle figures (hereby called signals, as this is done automatically when importing DXF files into copper layers) out of wires into respective polygons, keeping any curves correctly and allowing to replace individual wire coordinates with a straightforward yet simple syntax, which is also able to append additional coordinates. With that, it is even possible to put cutouts into the very same polygon.
5. The resulting XML polygons were than inserted into the eagle BRD file, replacing the wires generated during the DXF import.

I think this procedure is quite good for getting even complex vector logos into eagle, eventhough it is a bit annoying. Still, please note that eagle is a fantastic tool for creating multi-layer printed circuit board and is not made for the purpose to create great looking PCB art. But with their simple XML format eagle is very hacker approved in my opinion, without the need to bother with writing special ULPs or SCRipts.