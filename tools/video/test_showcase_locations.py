import unittest
from showcase_locations import choose
class LocationDrawTests(unittest.TestCase):
 def test_history_survives_batches(self):
  history={}
  a=choose([(str(i),'Nod')for i in range(7)],1,history)
  b=choose([(str(i),'GDI')for i in range(3)],2,history)
  for faction in ['GDI','Nod']:
   ids=[loc['id']for batch in [a,b]for pair in batch.values()for loc in [pair['opening'],pair['ending']]if loc['faction']==faction]
   self.assertEqual(len(ids),10);self.assertEqual(len(set(ids)),10)
 def test_refill_avoids_previous_location(self):
  history={}
  a=choose([(str(i),'Nod')for i in range(10)],4,history)
  last=a['9'];b=choose([('next','Nod')],5,history)['next']
  self.assertNotEqual(last['opening']['id'],b['opening']['id']);self.assertNotEqual(last['ending']['id'],b['ending']['id'])
if __name__=='__main__':unittest.main()
