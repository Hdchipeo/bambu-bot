import importlib.util
import json
import os
from pathlib import Path
import tempfile
import threading
import unittest
from http.server import ThreadingHTTPServer
from urllib.request import Request,urlopen
from urllib.error import HTTPError

spec=importlib.util.spec_from_file_location('knowledge',Path(__file__).resolve().parents[1]/'server/knowledge.py')
kbmod=importlib.util.module_from_spec(spec);spec.loader.exec_module(kbmod)

class KnowledgeTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.kb=kbmod.Knowledge(Path(self.tmp.name)/'test.sqlite3')
        self.url='https://wiki.bambulab.com/en/a1-mini/troubleshooting/nozzle-clog'
        self.kb.put_page(self.url,'Nozzle clog troubleshooting','Nozzle clogged. Cold pull filament cleaning. '+('Inspect the nozzle and use the official maintenance procedure. '*8))
    def tearDown(self):self.tmp.cleanup()
    def test_retrieval_sources_and_vietnamese_terms(self):
        hits=self.kb.search('đầu phun bị tắc')
        self.assertEqual(hits[0]['url'],self.url);self.assertIn('fetched_at',hits[0])
        self.assertEqual(self.kb.search('zzxxyyvvuunn'),[])
        self.assertEqual(self.kb.search('" OR * DROP TABLE sources'),[])
        self.kb.put_page(self.url,'Nozzle cleaning','Updated nozzle cleaning. '+('Maintenance procedure. '*10))
        self.assertEqual(self.kb.status()['wiki_pages'],1)
        self.assertNotIn('Cold pull',self.kb.search('nozzle')[0]['excerpt'])
    def test_hms_exact_lookup_unknown_and_auto_code_query(self):
        payload={'data':{'device_hms':{'en':[{'ecode':'0300010000010007','intro':'Nozzle heating abnormal'}]},
                         'device_error':{'en':[{'ecode':'03008001','intro':'Filament jam'}]}}}
        self.assertEqual(self.kb.import_hms(payload),2)
        found=self.kb.hms(['HMS_0300-0100-0001-0007','DEADBEEF'])
        self.assertEqual(found[0]['intro'],'Nozzle heating abnormal');self.assertTrue(found[0]['known'])
        self.assertFalse(found[1]['known']);self.assertIsNone(found[1]['intro'])
        self.assertEqual(self.kb.answer_context('HMS_0300-0100-0001-0007')['hms'][0]['code'],'0300010000010007')
        with self.assertRaises(ValueError):self.kb.hms(['not-a-code'])
    def test_source_allowlist_and_html(self):
        for url in ['file:///etc/passwd','http://wiki.bambulab.com/a','https://wiki.bambulab.com.evil.test/a','https://user@wiki.bambulab.com/a','https://127.0.0.1/a']:
            with self.assertRaises(ValueError):kbmod.official_url(url)
        p=kbmod.WikiHTML();p.feed('<title>Nozzle</title><script>ignore</script><nav>menu</nav><main><h1>Clog</h1><p>Clean nozzle safely</p></main>')
        self.assertIn('Clean nozzle safely',p.content());self.assertNotIn('ignore',p.content());self.assertNotIn('menu',p.content())
    def test_http_mcp_bridge(self):
        server=ThreadingHTTPServer(('127.0.0.1',0),kbmod.handler(self.kb,'test-token'))
        worker=threading.Thread(target=server.serve_forever,daemon=True);worker.start()
        base=f'http://127.0.0.1:{server.server_port}'
        try:
            self.assertEqual(json.load(urlopen(base+'/health'))['wiki_pages'],1)
            request=Request(base+'/v1/search',data=json.dumps({'query':'nozzle clog'}).encode(),headers={'Authorization':'Bearer test-token','Content-Type':'application/json'})
            response=json.load(urlopen(request));self.assertEqual(response['sources'][0]['url'],self.url)
            with self.assertRaises(HTTPError) as failure:urlopen(Request(base+'/v1/search',data=b'{}'))
            self.assertEqual(failure.exception.code,401);failure.exception.close()
            with self.assertRaises(HTTPError) as failure:urlopen(Request(base+'/v1/search',data=b'null',headers={'Authorization':'Bearer test-token'}))
            self.assertEqual(failure.exception.code,400);failure.exception.close()
        finally:server.shutdown();server.server_close();worker.join()
