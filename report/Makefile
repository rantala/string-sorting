default: report.html

algs.xml: generate-algs-xml Makefile
	./generate-algs-xml

report.html: Makefile htmlreport.xsl algs.xml sortable.js
	xsltproc htmlreport.xsl algs.xml >report.html

clean:
	rm -f algs.xml
	rm -f report.html
