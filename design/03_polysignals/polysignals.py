#!/bin/env python3
from xml.etree import ElementTree
import sys
import pprint

pp = pprint.PrettyPrinter(width=100)

tree = ElementTree.parse("polysignals.xml")
root = tree.getroot()
for signal in root.findall("signal"):
	layer = signal[0].attrib["layer"]
	width = signal[0].attrib["width"]
	polyelement = ElementTree.Element("polygon")
	polyelement.attrib["layer"] = layer
	polyelement.attrib["width"] = width
	wires = []
	for wire in signal.findall("wire"):
		wires.append({"c1": (wire.attrib["x1"], wire.attrib["y1"]), "c2": (wire.attrib["x2"], wire.attrib["y2"]), "curve": wire.attrib["curve"] if "curve" in wire.attrib else None})
		signal.remove(wire)

	if signal.text.strip():
		for spl in signal.text.strip().split("\n\n"):
			segs = [tuple(x.split("\t")) for x in spl.split("\n")]
			c1 = segs[0]
			c2 = segs[-1]
			new_wires = []
			last_seg = c1
			seg_count = 1
			for w2 in wires:
				if (w2["c1"] == c1 and w2["c2"] == c2) or (w2["c1"] == c2 and w2["c2"] == c1):
					for seg in segs[1:]:
						if seg in segs[0:seg_count]:
							break
						new_wires.append({"c1": last_seg, "c2": seg, "curve": None})
						seg_count = seg_count + 1
						last_seg = seg
				else:
					new_wires.append(w2)
			for seg in segs[seg_count:]:
				new_wires.append({"c1": last_seg, "c2": seg, "curve": None})
				last_seg = seg
			wires = new_wires

	#pp.pprint(wires)

	next_wire = wires[0]
	traversed = 1
	while True:
		vertex = ElementTree.SubElement(polyelement, "vertex")
		vertex.attrib["x"] = next_wire["c1"][0]
		vertex.attrib["y"] = next_wire["c1"][1]
		if next_wire["curve"] != None:
			vertex.attrib["curve"] = next_wire["curve"]
		next_wire["visited"] = True
		if traversed > 1 and (next_wire["c1"] == wires[0]["c1"] or next_wire["c2"] == wires[0]["c1"]):
			if traversed < len(wires):
				print("Warning: There are too many wires in signal named \""+signal.attrib["name"]+"\".")
				pp.pprint([x for x in wires if not "visited" in x])
			break # finished, we have found all wires forming a closed path
		traversed = traversed + 1
		cur_wire = next_wire
		for w2 in wires:
			if "visited" in w2:
				continue
			if next_wire["c2"] == w2["c1"]:
				next_wire = w2
				break
			elif next_wire["c2"] == w2["c2"]:
				c3 = w2["c2"]
				w2["c2"] = w2["c1"]
				w2["c1"] = c3
				if w2["curve"] != None:
					w2["curve"] = str(-float(w2["curve"]))
				next_wire = w2
				break
		if next_wire == cur_wire:
			print("Error: all signals must contain only one closed path. (Failed Signal: "+signal.attrib["name"]+", couldn't find next point for "+str(next_wire["c2"])+")")
			break
	signal.append(polyelement)

def indent(elem, level=0):
	elem.tail = "\n"
	if len(elem):
		elem.text = "\n"
		for subelem in elem:
			indent(subelem)
	return elem

indent(root)
tree.write("polysignals_out.xml")