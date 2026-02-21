#pragma once

/* XML defaults embedded at build time by codegen.py.
 * Call provision_spiffs_xml() once in setup() after SPIFFS.begin()
 * to ensure these files are always present after flashing. */

static const char METADATA_XML_DEFAULT[] = R"XMLRAW(
<Metadata>
<row><Key>0</Key><Message>choice</Message><Class>type</Class></row>
<row><Key>1</Key><Message>numeric</Message><Class>type</Class></row>
<row><Key>1</Key><Message>bar</Message><Class>pressure</Class></row>
<row><Key>1</Key><Message>Less than</Message><Class>Operation_ID</Class></row>
<row><Key>1</Key><Message>TOO LOW</Message><Class>Fault_Code</Class></row>
</Metadata>
)XMLRAW";

static const char VARIABLES_XML_DEFAULT[] = R"XMLRAW(
<Variables>
<row><Class>1</Class><Type>2</Type><Name>1</Name><Initial_Value>2</Initial_Value><Operation_ID>1</Operation_ID><Threshold>2</Threshold><Increment>2</Increment><Slave_ID>2</Slave_ID><Function_ID>1</Function_ID><Start_Address>2</Start_Address><Length>5</Length></row>
</Variables>
)XMLRAW";
