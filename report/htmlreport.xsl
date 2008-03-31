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
			<body>
				<!-- allows filtering algorithm names -->
				<xsl:text>Filter:</xsl:text>
				<form>
					<input name="filter"
					       onkeyup="filter2(this, 'genome');
						filter2(this, 'nodup');
						filter2(this, 'url');" type="text"/>
				</form>
				<center>
					<table class="sortable" id="genome" summary="Comparison of algorithm performance with genome dataset.">
						<caption>genome3</caption>
						<xsl:call-template name="output-table-header"/>
						<xsl:call-template name="read-info">
							<xsl:with-param name="input">genome3</xsl:with-param>
						</xsl:call-template>
					</table>
					<table class="sortable" id="nodup" summary="Comparison of algorithm performance with no-duplicates dataset.">
						<caption>nodup3</caption>
						<xsl:call-template name="output-table-header"/>
						<xsl:call-template name="read-info">
							<xsl:with-param name="input">nodup3</xsl:with-param>
						</xsl:call-template>
					</table>
					<table class="sortable" id="url" summary="Comparison of algorithm performance with URL dataset.">
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
		</tr>
	</xsl:template>
	<xsl:template name="read-info">
		<xsl:param name="input"/>
		<xsl:for-each select="/algs/algnum">
			<xsl:call-template name="read-info-alg">
				<xsl:with-param name="input" select="$input"/>
				<xsl:with-param name="alg" select="@value"/>
			</xsl:call-template>
		</xsl:for-each>
	</xsl:template>
	<xsl:template name="read-info-alg">
		<xsl:param name="input"/>
		<xsl:param name="alg"/>
		<tr>
			<xsl:apply-templates select="document(concat('data/timings_',  $input, '_', $alg, '.xml'))"/>
			<xsl:apply-templates select="document(concat('data/oprofile_', $input, '_', $alg, '.xml'))"/>
		</tr>
	</xsl:template>
	<!-- these come from the timings data -->
	<xsl:template match="/event">
		<td><xsl:value-of select="algorithm/@num"/></td>
		<td><xsl:value-of select="algorithm/@name"/></td>
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
</xsl:stylesheet>
