<!--
  Copyright 2008 by Tommi Rantala <tommi.rantala@cs.helsinki.fi>
 
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to
  deal in the Software without restriction, including without limitation the
  rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
  sell copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:
 
  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.
 
  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
  IN THE SOFTWARE.
-->
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
	<xsl:output method="html"/>
	<xsl:strip-space elements="*"/>
	<xsl:template match="/algs">
		<html>
			<script src="sortable.js" type="text/javascript"/>
			<style type="text/css">
				table.sortable {
					border-spacing: 0;
					border: 1px solid #000;
					border-collapse: collapse;
				}
				table.sortable th, table.sortable td {
					text-align: left;
					padding: 2px 4px 2px 4px;
					width: 100px;
					border-style: solid;
					border-color: #444;
				}
				table.sortable th {
					border-width: 0px 1px 1px 1px;
					background-color: #ccc;
				}
				table.sortable td {
					border-width: 0px 1px 0px 1px;
				}
				table.sortable tr.odd td {
					background-color: #ddd;
				}
				table.sortable tr.even td {
					background-color: #fff;
				}
				table.sortable tr.sortbottom td {
					border-top: 1px solid #444;
					background-color: #ccc;
					font-weight: bold;
				}
			</style>
			<!-- source: http://www.vonloesch.de/node/23 -->
			<script type="text/javascript">
				<![CDATA[
				function filter2 (phrase, _id){
					var words = phrase.value.toLowerCase().split(" ");
					var table = document.getElementById(_id);
					var ele;
					for (var r = 1; r < table.rows.length; r++){
						ele = table.rows[r].innerHTML.replace(/<[^>]+>/g,"");
						var displayStyle = 'none';
						for (var i = 0; i < words.length; i++) {
						    if (ele.toLowerCase().indexOf(words[i])>=0)
							displayStyle = '';
						    else {
							displayStyle = 'none';
							break;
						    }
						}
						table.rows[r].style.display = displayStyle;
					}
				}
				]]>
			</script>
			<script type="text/javascript">
				<![CDATA[
				function hidetable (_id) {
					var table = document.getElementById(_id);
					if (table.style.display.indexOf('none') >= 0) {
						table.style.display = '';
					} else {
						table.style.display = 'none';
					}
				}
				]]>
			</script>
			<body>
				<!-- allows filtering algorithm names -->
				<form>
					<xsl:text>Filter: </xsl:text>
					<input name="filter"
					       onkeyup="filter2(this, 'genome3');
						filter2(this, 'nodup3');
						filter2(this, 'url3');" type="text"/>
				</form>
				<form>
					<xsl:text>Show/hide tables: </xsl:text>
					<button type="button" onclick="hidetable('genome3')">genome3</button>
					<button type="button" onclick="hidetable('nodup3')">nodup3</button>
					<button type="button" onclick="hidetable('url3')">url3</button>
				</form>
				<center>
					<table class="sortable" id="genome3" summary="Comparison of algorithm performance with genome dataset.">
						<caption>genome3</caption>
						<xsl:call-template name="output-table-header"/>
						<xsl:call-template name="read-info">
							<xsl:with-param name="input">genome3</xsl:with-param>
						</xsl:call-template>
					</table>
					<table class="sortable" id="nodup3" summary="Comparison of algorithm performance with no-duplicates dataset.">
						<caption>nodup3</caption>
						<xsl:call-template name="output-table-header"/>
						<xsl:call-template name="read-info">
							<xsl:with-param name="input">nodup3</xsl:with-param>
						</xsl:call-template>
					</table>
					<table class="sortable" id="url3" summary="Comparison of algorithm performance with URL dataset.">
						<caption>url3</caption>
						<xsl:call-template name="output-table-header"/>
						<xsl:call-template name="read-info">
							<xsl:with-param name="input">url3</xsl:with-param>
						</xsl:call-template>
					</table>
				</center>
			</body>
		</html>
	</xsl:template>
	<xsl:template name="output-table-header">
		<tr>
			<th>ID</th>
			<th>Name</th>
			<th>Time (ms)</th>
			<th>Clock cycles x10^6</th>
			<th>Instructions x10^6</th>
			<th>CPI</th>
			<th>DTLB misses x10^6</th>
			<th>L1 cache line misses x10^6</th>
			<th>L2 cache line misses x10^6</th>
			<th>Load blocks x10^6</th>
			<th>Store order blocks x10^6</th>
			<th>Heap peak (megabytes)</th>
			<th>{m,c,re}alloc calls</th>
		</tr>
	</xsl:template>
	<xsl:template name="read-info">
		<xsl:param name="input"/>
		<xsl:for-each select="/algs/alg">
			<tr>
				<td><xsl:value-of select="@algnum"/></td>
				<td><xsl:value-of select="@algname"/></td>
				<xsl:apply-templates select="document(concat('data/timings_',  $input, '_', @algnum, '.xml'))"/>
				<xsl:apply-templates select="document(concat('data/oprofile_', $input, '_', @algnum, '.xml'))"/>
				<xsl:apply-templates select="document(concat('data/memusage_', $input, '_', @algnum, '.xml'))"/>
			</tr>
		</xsl:for-each>
	</xsl:template>
	<!-- these come from the timings data -->
	<xsl:template match="/event">
		<td><xsl:value-of select="time/@seconds"/></td>
	</xsl:template>
	<!-- these come from the oprofile data -->
	<xsl:template match="/simple">
		<td><xsl:value-of select="format-number(event[@name='CPU_CLK_UNHALTED']/@value div 1e6,   '#')"/></td>
		<td><xsl:value-of select="format-number(event[@name='INST_RETIRED.ANY_P']/@value div 1e6, '#')"/></td>
		<td><xsl:value-of
				select="format-number(event[@name='CPU_CLK_UNHALTED']/@value div
				event[@name='INST_RETIRED.ANY_P']/@value, '#.##')"/></td>
		<td><xsl:value-of select="format-number(event[@name='DTLB_MISSES']/@value div 1e6, '#')"/></td>
		<td><xsl:value-of select="format-number(event[@name='MEM_LOAD_RETIRED' and @mask='2']/@value div 1e6, '#')"/></td>
		<td><xsl:value-of select="format-number(event[@name='MEM_LOAD_RETIRED' and @mask='8']/@value div 1e6, '#')"/></td>
		<td><xsl:value-of select="format-number(
				 (event[@name='LOAD_BLOCK' and @mask='2']/@value +
				  event[@name='LOAD_BLOCK' and @mask='4']/@value +
				  event[@name='LOAD_BLOCK' and @mask='8']/@value +
				  event[@name='LOAD_BLOCK' and @mask='16']/@value +
				  event[@name='LOAD_BLOCK' and @mask='32']/@value) div 1e6,
				'#')"/></td>
		<td><xsl:value-of select="format-number(event[@name='STORE_BLOCK']/@value div 1e6, '#')"/></td>
	</xsl:template>
	<!-- memusage data -->
	<xsl:template match="/memusage/event">
		<td><xsl:value-of select="format-number(@heap-peak div 1048576, '#')"/></td>
		<td><xsl:value-of select="format-number(@calls-malloc + @calls-realloc + @calls-calloc, '#')"/></td>
	</xsl:template>
</xsl:stylesheet>
