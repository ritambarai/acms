#pragma once

/* XML defaults embedded at build time by codegen.py.
 * Call provision_spiffs_xml() once in setup() after SPIFFS.begin()
 * to ensure these files are always present after flashing. */

static const char METADATA_XML_DEFAULT[] = R"XMLRAW(
<Metadata>
<row><Class>Fault_Code</Class><Key>101</Key><Message>Fault</Message></row>
<row><Class>Fault_Code</Class><Key>201</Key><Message>Low Level</Message></row>
<row><Class>Fault_Code</Class><Key>301</Key><Message>Over Voltage</Message></row>
<row><Class>Fault_Code</Class><Key>302</Key><Message>Under Voltage</Message></row>
<row><Class>Fault_Code</Class><Key>401</Key><Message>Low Pressure</Message></row>
<row><Class>Fault_Code</Class><Key>402</Key><Message>High Pressure</Message></row>
<row><Class>level</Class><Key>1</Key><Message>Percentage(%)</Message></row>
<row><Class>level</Class><Key>2</Key><Message>meter</Message></row>
<row><Class>mode</Class><Key>0</Key><Message>AUTO</Message></row>
<row><Class>mode</Class><Key>1</Key><Message>MANUAL</Message></row>
<row><Class>Operation_ID</Class><Key>0</Key><Message>Equals to</Message></row>
<row><Class>Operation_ID</Class><Key>1</Key><Message>Less than</Message></row>
<row><Class>Operation_ID</Class><Key>2</Key><Message>Greater than</Message></row>
<row><Class>Operation_ID</Class><Key>3</Key><Message>Greater than/ Equals to</Message></row>
<row><Class>Operation_ID</Class><Key>4</Key><Message>Less than/ Equals to</Message></row>
<row><Class>Operation_ID</Class><Key>5</Key><Message>Not Equals to</Message></row>
<row><Class>pressure</Class><Key>1</Key><Message>bar</Message></row>
<row><Class>status</Class><Key>0</Key><Message>OFF</Message></row>
<row><Class>status</Class><Key>1</Key><Message>ON</Message></row>
<row><Class>trip</Class><Key>0</Key><Message>FALSE</Message></row>
<row><Class>trip</Class><Key>1</Key><Message>TRUE</Message></row>
<row><Class>type</Class><Key>0</Key><Message>choice</Message></row>
<row><Class>type</Class><Key>1</Key><Message>numeric</Message></row>
<row><Class>voltage</Class><Key>1</Key><Message>Volts(V)</Message></row>
</Metadata>
)XMLRAW";

static const char VARIABLES_XML_DEFAULT[] = R"XMLRAW(
<Variables>
<row><Class>diesel tanks</Class><Name>level</Name><Type>type</Type><Value>1</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID/><Threshold/><Fault_Code/><Increment/></row>
<row><Class>diesel tanks</Class><Name>level</Name><Type>unit</Type><Value>1</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID/><Threshold/><Fault_Code/><Increment/></row>
<row><Class>diesel tanks</Class><Name>level</Name><Type>main</Type><Value>50</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID>4</Operation_ID><Threshold>10</Threshold><Fault_Code>201</Fault_Code><Increment/></row>
<row><Class>pipes</Class><Name>pressure</Name><Type>type</Type><Value>1</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID/><Threshold/><Fault_Code/><Increment/></row>
<row><Class>pipes</Class><Name>pressure</Name><Type>unit</Type><Value>1</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID/><Threshold/><Fault_Code/><Increment/></row>
<row><Class>pipes</Class><Name>pressure</Name><Type>booster</Type><Value>5</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID>1</Operation_ID><Threshold>1</Threshold><Fault_Code>401</Fault_Code><Increment/></row>
<row><Class>pipes</Class><Name>pressure</Name><Type>fire hose</Type><Value>5</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID>1</Operation_ID><Threshold>1</Threshold><Fault_Code>401</Fault_Code><Increment/></row>
<row><Class>pipes</Class><Name>pressure</Name><Type>hydrant</Type><Value>5</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID>1</Operation_ID><Threshold>1</Threshold><Fault_Code>401</Fault_Code><Increment/></row>
<row><Class>pipes</Class><Name>pressure</Name><Type>main line</Type><Value>5</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID>1</Operation_ID><Threshold>1</Threshold><Fault_Code>401</Fault_Code><Increment/></row>
<row><Class>pipes</Class><Name>pressure</Name><Type>sprinkler</Type><Value>5</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID>1</Operation_ID><Threshold>1</Threshold><Fault_Code>401</Fault_Code><Increment/></row>
<row><Class>pipes</Class><Name>pressure</Name><Type>stand pipe</Type><Value>5</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID>1</Operation_ID><Threshold>1</Threshold><Fault_Code>401</Fault_Code><Increment/></row>
<row><Class>pumps</Class><Name>mode</Name><Type>type</Type><Value>0</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID/><Threshold/><Fault_Code/><Increment/></row>
<row><Class>pumps</Class><Name>mode</Name><Type>booster</Type><Value>0</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID/><Threshold/><Fault_Code/><Increment/></row>
<row><Class>pumps</Class><Name>mode</Name><Type>diesel</Type><Value>0</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID/><Threshold/><Fault_Code/><Increment/></row>
<row><Class>pumps</Class><Name>mode</Name><Type>jockey</Type><Value>0</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID/><Threshold/><Fault_Code/><Increment/></row>
<row><Class>pumps</Class><Name>mode</Name><Type>main</Type><Value>0</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID/><Threshold/><Fault_Code/><Increment/></row>
<row><Class>pumps</Class><Name>status</Name><Type>type</Type><Value>0</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID/><Threshold/><Fault_Code/><Increment/></row>
<row><Class>pumps</Class><Name>status</Name><Type>booster</Type><Value>0</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID/><Threshold/><Fault_Code/><Increment/></row>
<row><Class>pumps</Class><Name>status</Name><Type>diesel</Type><Value>0</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID/><Threshold/><Fault_Code/><Increment/></row>
<row><Class>pumps</Class><Name>status</Name><Type>jockey</Type><Value>0</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID/><Threshold/><Fault_Code/><Increment/></row>
<row><Class>pumps</Class><Name>status</Name><Type>main</Type><Value>0</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID/><Threshold/><Fault_Code/><Increment/></row>
<row><Class>pumps</Class><Name>trip</Name><Type>type</Type><Value>0</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID/><Threshold/><Fault_Code/><Increment/></row>
<row><Class>pumps</Class><Name>trip</Name><Type>booster</Type><Value>0</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID>0</Operation_ID><Threshold>1</Threshold><Fault_Code>101</Fault_Code><Increment/></row>
<row><Class>pumps</Class><Name>trip</Name><Type>diesel</Type><Value>0</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID>0</Operation_ID><Threshold>1</Threshold><Fault_Code>101</Fault_Code><Increment/></row>
<row><Class>pumps</Class><Name>trip</Name><Type>jockey</Type><Value>0</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID>0</Operation_ID><Threshold>1</Threshold><Fault_Code>101</Fault_Code><Increment/></row>
<row><Class>pumps</Class><Name>trip</Name><Type>main</Type><Value>0</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID>0</Operation_ID><Threshold>1</Threshold><Fault_Code>101</Fault_Code><Increment/></row>
<row><Class>pumps</Class><Name>trip</Name><Type>test</Type><Value>0</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID>0</Operation_ID><Threshold>1</Threshold><Fault_Code>101</Fault_Code><Increment>1</Increment></row>
<row><Class>pumps</Class><Name>voltage</Name><Type>type</Type><Value>1</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID/><Threshold/><Fault_Code/><Increment/></row>
<row><Class>pumps</Class><Name>voltage</Name><Type>unit</Type><Value>1</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID/><Threshold/><Fault_Code/><Increment/></row>
<row><Class>pumps</Class><Name>voltage</Name><Type>diesel</Type><Value>240</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID>2</Operation_ID><Threshold>270</Threshold><Fault_Code>301</Fault_Code><Increment/></row>
<row><Class>pumps</Class><Name>voltage</Name><Type>diesel</Type><Value>240</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID>1</Operation_ID><Threshold>200</Threshold><Fault_Code>302</Fault_Code><Increment/></row>
<row><Class>pumps</Class><Name>voltage</Name><Type>jockey</Type><Value>240</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID>2</Operation_ID><Threshold>270</Threshold><Fault_Code>301</Fault_Code><Increment/></row>
<row><Class>pumps</Class><Name>voltage</Name><Type>jockey</Type><Value>240</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID>1</Operation_ID><Threshold>200</Threshold><Fault_Code>302</Fault_Code><Increment/></row>
<row><Class>pumps</Class><Name>voltage</Name><Type>main</Type><Value>240</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID>2</Operation_ID><Threshold>270</Threshold><Fault_Code>301</Fault_Code><Increment/></row>
<row><Class>pumps</Class><Name>voltage</Name><Type>main</Type><Value>240</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID>1</Operation_ID><Threshold>200</Threshold><Fault_Code>302</Fault_Code><Increment/></row>
<row><Class>pumps</Class><Name>voltage</Name><Type>test</Type><Value>240</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID>2</Operation_ID><Threshold>270</Threshold><Fault_Code>301</Fault_Code><Increment>5</Increment></row>
<row><Class>water tanks</Class><Name>level</Name><Type>type</Type><Value>1</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID/><Threshold/><Fault_Code/><Increment/></row>
<row><Class>water tanks</Class><Name>level</Name><Type>unit</Type><Value>1</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID/><Threshold/><Fault_Code/><Increment/></row>
<row><Class>water tanks</Class><Name>level</Name><Type>Overground A</Type><Value>50</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID>4</Operation_ID><Threshold>10</Threshold><Fault_Code>201</Fault_Code><Increment/></row>
<row><Class>water tanks</Class><Name>level</Name><Type>Overground B</Type><Value>50</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID>4</Operation_ID><Threshold>10</Threshold><Fault_Code>201</Fault_Code><Increment/></row>
<row><Class>water tanks</Class><Name>level</Name><Type>Underground A</Type><Value>50</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID>4</Operation_ID><Threshold>10</Threshold><Fault_Code>201</Fault_Code><Increment/></row>
<row><Class>water tanks</Class><Name>level</Name><Type>Underground B</Type><Value>50</Value><Slave_ID/><Function_ID/><Start_Address/><Data_Length/><Operation_ID>4</Operation_ID><Threshold>10</Threshold><Fault_Code>201</Fault_Code><Increment/></row>
</Variables>
)XMLRAW";

static const char SETTINGS_XML_DEFAULT[] = R"XMLRAW(
<Settings>
  <row><general><SSID>Ritam iPhone</SSID><Password>password</Password><Class_Pool_Size>32</Class_Pool_Size><Var_Pool_Size>128</Var_Pool_Size></general></row>
  <row><mqtt><Host>mqtt-server.ddns.net</Host><Port>1883</Port><Data_Topic>ACMS_Rey/01/</Data_Topic><Alert_Topic>ACMS_Rey/01/</Alert_Topic><Username></Username><Mqtt_Password></Mqtt_Password></mqtt></row>
  <row><json_includes><Metadata>true</Metadata><Constraints>true</Constraints><Type_Unit>true</Type_Unit></json_includes></row>
</Settings>
)XMLRAW";
