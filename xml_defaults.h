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
<row><Class>sensor</Class><Name>pressure</Name><Type>main</Type><Value>1.54</Value><Operation_ID>1</Operation_ID><Threshold>10</Threshold><Fault_Code>1</Fault_Code><Increment/><Slave_ID/><Function_ID/><Start_Address/><Data_Length/></row>
<row><Class>sensor</Class><Name>pressure</Name><Type>type</Type><Value>1</Value><Operation_ID/><Threshold/><Fault_Code/><Increment/><Slave_ID/><Function_ID/><Start_Address/><Data_Length/></row>
<row><Class>sensor</Class><Name>pressure</Name><Type>unit</Type><Value>1</Value><Operation_ID/><Threshold/><Fault_Code/><Increment/><Slave_ID/><Function_ID/><Start_Address/><Data_Length/></row>
<row><Class>sensor</Class><Name>status</Name><Type>unit</Type><Value>1</Value><Operation_ID/><Threshold/><Fault_Code/><Increment/><Slave_ID/><Function_ID/><Start_Address/><Data_Length/></row>
<row><Class>sensor</Class><Name>pressure</Name><Type>side</Type><Value>1.54</Value><Operation_ID/><Threshold/><Fault_Code/><Increment>0.01</Increment><Slave_ID/><Function_ID/><Start_Address/><Data_Length/></row>
</Variables>
)XMLRAW";

static const char SETTINGS_XML_DEFAULT[] = R"XMLRAW(
<Settings>
  <row><general><SSID>Airtel_dish_8109</SSID><Password>Air@39818</Password><Class_Pool_Size>32</Class_Pool_Size><Var_Pool_Size>128</Var_Pool_Size></general></row>
  <row><mqtt><Host>mqtt-server.ddns.net</Host><Port>1883</Port><Data_Topic>ACMS_Rey/01/</Data_Topic><Alert_Topic></Alert_Topic><Username></Username><Mqtt_Password></Mqtt_Password></mqtt></row>
  <row><json><Metadata>false</Metadata><Constraints>false</Constraints><Modbus>false</Modbus></json></row>
</Settings>
)XMLRAW";
