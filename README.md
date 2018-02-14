# xAPI converter

Application that convertes moodle logs into xAPI statements.

anssi.grohn@karelia.fi (c) 2018.

## MongoDB related stuff on server

Looking at statements

```
# mongo
# use learninglocker_v2
# db.statements.find()
```

Drop all statements
```
# mongo
# use learninglocker_v2
# db.statements.remove({})
```

## Related documentation

[Submitting discussion statements ](http://xapiquarterly.com/2016/10/instrumenting-xapi-in-forums-discussion-groups/)
