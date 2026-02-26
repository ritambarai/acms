#pragma once

/* XML defaults embedded at build time by codegen.py.
 * Call provision_spiffs_xml() once in setup() after SPIFFS.begin()
 * to ensure these files are always present after flashing. */

static const char METADATA_XML_DEFAULT[] = R"XMLRAW(
<Metadata>
<row><Class>type</Class><Key>0</Key><Message>choice</Message></row>
<row><Class>type</Class><Key>1</Key><Message>numeric</Message></row>
<row><Class>pressure</Class><Key>1</Key><Message>bar</Message></row>
<row><Class>Operation_ID</Class><Key>1</Key><Message>Less than</Message></row>
<row><Class>Operation_ID</Class><Key>2</Key><Message>Greater than</Message></row>
<row><Class>Operation_ID</Class><Key>3</Key><Message>Greater than/ Equals to</Message></row>
<row><Class>Operation_ID</Class><Key>4</Key><Message>Less than/ Equals to</Message></row>
<row><Class>Operation_ID</Class><Key>0</Key><Message>Equals to</Message></row>
<row><Class>Operation_ID</Class><Key>5</Key><Message>Not Equals to</Message></row>
<row><Class>level</Class><Key>1</Key><Message>Percentage(%)</Message></row>
<row><Class>status</Class><Key>0</Key><Message>OFF</Message></row>
<row><Class>status</Class><Key>1</Key><Message>ON</Message></row>
<row><Class>mode</Class><Key>0</Key><Message>AUTO</Message></row>
<row><Class>mode</Class><Key>1</Key><Message>MANUAL</Message></row>
<row><Class>level</Class><Key>1</Key><Message>meter</Message></row>
<row><Class>trip</Class><Key>0</Key><Message>FALSE</Message></row>
<row><Class>trip</Class><Key>1</Key><Message>TRUE</Message></row>
<row><Class>Fault_Code</Class><Key>201</Key><Message>Low Level</Message></row>
<row><Class>Fault_Code</Class><Key>401</Key><Message>Low Pressure</Message></row>
<row><Class>Fault_Code</Class><Key>402</Key><Message>High Pressure</Message></row>
<row><Class>Fault_Code</Class><Key>100</Key><Message>Fault</Message></row>
<row><Class>voltage</Class><Key>1</Key><Message>Volts(V)</Message></row>
<row><Class>Fault_Code</Class><Key>301</Key><Message>Over Voltage</Message></row>
<row><Class>Fault_Code</Class><Key>302</Key><Message>Under Voltage</Message></row>
<row><Class>Fault_Code</Class><Key>101</Key><Message>Fault</Message></row>
</Metadata>
)XMLRAW";

static const char VARIABLES_XML_DEFAULT[] = R"XMLRAW(
<Variables>
<row><Class>pumps</Class><Type>type</Type><Name>voltage</Name><Value>1</Value><Operation_ID/><Threshold/><Fault_Code/><Increment/><Slave_ID/><Function_ID/><Start_Address/><Data_Length/></row>
<row><Class>pumps</Class><Type>unit</Type><Name>voltage</Name><Value>1</Value><Operation_ID/><Threshold/><Fault_Code/><Increment/><Slave_ID/><Function_ID/><Start_Address/><Data_Length/></row>
<row><Class>pumps</Class><Type>main</Type><Name>voltage</Name><Value>240</Value><Operation_ID>2</Operation_ID><Threshold>270</Threshold><Fault_Code>301</Fault_Code><Increment/><Slave_ID/><Function_ID/><Start_Address/><Data_Length/></row>
<row><Class>pumps</Class><Type>main</Type><Name>voltage</Name><Value>240</Value><Operation_ID>1</Operation_ID><Threshold>200</Threshold><Fault_Code>302</Fault_Code><Increment/><Slave_ID/><Function_ID/><Start_Address/><Data_Length/></row>
<row><Class>pumps</Class><Type>type</Type><Name>status</Name><Value>0</Value><Operation_ID/><Threshold/><Fault_Code/><Increment/><Slave_ID/><Function_ID/><Start_Address/><Data_Length/></row>
<row><Class>pumps</Class><Type>type</Type><Name>mode</Name><Value>0</Value><Operation_ID/><Threshold/><Fault_Code/><Increment/><Slave_ID/><Function_ID/><Start_Address/><Data_Length/></row>
<row><Class>pumps</Class><Type>type</Type><Name>trip</Name><Value>0</Value><Operation_ID/><Threshold/><Fault_Code/><Increment/><Slave_ID/><Function_ID/><Start_Address/><Data_Length/></row>
<row><Class>pumps</Class><Type>jockey</Type><Name>voltage</Name><Value>240</Value><Operation_ID>2</Operation_ID><Threshold>270</Threshold><Fault_Code>301</Fault_Code><Increment/><Slave_ID/><Function_ID/><Start_Address/><Data_Length/></row>
<row><Class>pumps</Class><Type>jockey</Type><Name>voltage</Name><Value>240</Value><Operation_ID>1</Operation_ID><Threshold>200</Threshold><Fault_Code>302</Fault_Code><Increment/><Slave_ID/><Function_ID/><Start_Address/><Data_Length/></row>
</Variables>
)XMLRAW";

static const char SETTINGS_XML_DEFAULT[] = R"XMLRAW(
<Settings>
  <row><general><SSID>Airtel_kaus_0496</SSID><Password>air68987</Password><Class_Pool_Size>32</Class_Pool_Size><Var_Pool_Size>128</Var_Pool_Size></general></row>
  <row><mqtt><Host>mqtt-server.ddns.net</Host><Port>1883</Port><Data_Topic>ACMS_Rey/01/</Data_Topic><Alert_Topic>ACMS_Rey/01/</Alert_Topic><Username></Username><Mqtt_Password></Mqtt_Password></mqtt></row>
  <row><json><Metadata>true</Metadata><Constraints>true</Constraints><Modbus>false</Modbus></json></row>
</Settings>
)XMLRAW";
