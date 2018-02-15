# xAPI converter

Application that convertes moodle logs into xAPI statements.

Anssi Gröhn (c) 2018.

(firstname lastname at karelia dot fi)

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
[xAPI Verb registry](https://registry.tincanapi.com/)
[Activity Schema](https://github.com/activitystreams/activity-schema/blob/master/activity-schema.md)
